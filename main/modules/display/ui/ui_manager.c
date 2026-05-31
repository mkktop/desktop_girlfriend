/**
 * @file ui_manager.c
 * @brief UI页面管理器实现（三层容器架构）
 * @author mkk
 * @date 2026-05-31
 * @note 三层容器架构：
 *       lv_screen_active()
 *         └── sys_container（系统层，永不销毁）
 *               ├── status_bar（WiFi状态 + 时钟，始终可见）
 *               └── page_container（页面层，承载当前功能页）
 *       页面切换通过 lv_obj_clean() 一行清空旧页面，
 *       新页面在 page_container 下创建组件
 */

#include "ui_manager.h"
#include "ui_home.h"
#include "ui_wifi_config.h"
#include "app_event.h"
#include "app_wifi.h"
#include "app_font.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include <time.h>

#define TAG "ui_manager"

/* ====== 容器和系统级组件 ====== */

static lv_obj_t *s_sys_container = NULL;    /* 系统层容器 */
static lv_obj_t *s_status_label = NULL;     /* WiFi 状态标签 */
static lv_obj_t *s_clock_label = NULL;      /* 时钟标签 */
static lv_obj_t *s_page_container = NULL;   /* 页面层容器 */

/* ====== 页面状态 ====== */

static ui_page_id_t s_current_page = UI_PAGE_COUNT; /* 未初始化 */

/* 页面注册表：每个页面的 create 和 on_event */
static const ui_page_interface_t s_pages[UI_PAGE_COUNT] = {
    [UI_PAGE_HOME]        = { .create = ui_home_create,        .on_event = ui_home_on_event },
    [UI_PAGE_WIFI_CONFIG] = { .create = ui_wifi_config_create, .on_event = NULL },
};

/* ====== 页面切换 ====== */

/**
 * @brief 切换页面
 * @note 清空 page_container 并创建新页面，系统层不受影响
 */
void ui_manager_switch_page(ui_page_id_t page)
{
    if (page >= UI_PAGE_COUNT || page == s_current_page) {
        return;
    }

    ESP_LOGI(TAG, "Switching page: %d -> %d", s_current_page, page);

    /* 清空页面容器（一行删除旧页面所有子对象） */
    lv_obj_clean(s_page_container);

    /* 创建新页面 */
    s_pages[page].create(s_page_container);
    s_current_page = page;
}

/* ====== 事件处理 ====== */

/**
 * @brief 事件观察者回调，更新 UI 状态
 * @param event_bits 触发的事件位
 * @param user_ctx   未使用
 */
static void ui_event_handler(EventBits_t event_bits, void *user_ctx)
{
    (void)user_ctx;

    if (!lvgl_port_lock(0)) {
        return;
    }

    /* 系统级事件：更新状态栏 */
    if (event_bits & APP_EVENT_WIFI_CONNECTED) {
        if (s_status_label) {
            lv_label_set_text(s_status_label, "WiFi: \xe8\xbf\x9e\xe6\x8e\xa5\xe4\xb8\xad..."); /* 连接中... */
        }
    }
    if (event_bits & APP_EVENT_WIFI_GOT_IP) {
        if (s_status_label) {
            char text[64];
            snprintf(text, sizeof(text), "WiFi: IP %s", app_wifi_get_ip());
            lv_label_set_text(s_status_label, text);
        }
    }
    if (event_bits & APP_EVENT_WIFI_DISCONNECTED) {
        if (s_status_label) {
            lv_label_set_text(s_status_label, "WiFi: \xe6\x9c\xaa\xe8\xbf\x9e\xe6\x8e\xa5"); /* 未连接 */
        }
    }

    /* 系统级事件：更新时钟 */
    if (event_bits & APP_EVENT_TIME_SYNCED) {
        if (s_clock_label) {
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            char time_str[16];
            strftime(time_str, sizeof(time_str), "%H:%M", tm_info);
            lv_label_set_text(s_clock_label, time_str);
        }
    }

    /* 配网模式切换（由 ui_manager 控制页面切换） */
    if (event_bits & APP_EVENT_WIFI_CONFIG_ENTER) {
        if (s_status_label) {
            lv_label_set_text(s_status_label, "WiFi: \xe9\x85\x8d\xe7\xbd\x91\xe6\xa8\xa1\xe5\xbc\x8f"); /* 配网模式 */
        }
        ui_manager_switch_page(UI_PAGE_WIFI_CONFIG);
    }
    if (event_bits & APP_EVENT_WIFI_CONFIG_EXIT) {
        ui_manager_switch_page(UI_PAGE_HOME);
    }

    /* 页面级事件：分发给当前页面 */
    if (s_current_page < UI_PAGE_COUNT && s_pages[s_current_page].on_event) {
        s_pages[s_current_page].on_event(event_bits);
    }

    lvgl_port_unlock();
}

/* ====== 公开函数 ====== */

void ui_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing UI manager...");

    /* 注册事件观察者 */
    const EventBits_t event_bits =
        APP_EVENT_WIFI_CONNECTED | APP_EVENT_WIFI_DISCONNECTED | APP_EVENT_WIFI_GOT_IP
      | APP_EVENT_WIFI_CONFIG_ENTER | APP_EVENT_WIFI_CONFIG_EXIT
      | APP_EVENT_TIME_SYNCED;
    app_event_register_handler(ui_event_handler, event_bits, NULL);

    /* 在锁保护下创建三层容器 */
    if (!lvgl_port_lock(pdMS_TO_TICKS(5000))) {
        ESP_LOGE(TAG, "Failed to lock LVGL for init");
        return;
    }

    /* 系统层容器（全屏，无边框，不可滚动） */
    s_sys_container = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(s_sys_container);
    lv_obj_set_size(s_sys_container, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_sys_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(s_sys_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 状态栏（系统层，始终可见） */
    s_status_label = lv_label_create(s_sys_container);
    lv_label_set_text(s_status_label, "WiFi: \xe7\xad\x89\xe5\xbe\x85\xe8\xbf\x9e\xe6\x8e\xa5..."); /* 等待连接... */
    lv_obj_set_style_text_font(s_status_label, app_font_get_text(), 0);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_LEFT, 5, 5);

    /* 时钟标签（系统层，始终可见） */
    s_clock_label = lv_label_create(s_sys_container);
    lv_label_set_text(s_clock_label, "--:--");
    lv_obj_set_style_text_font(s_clock_label, app_font_get_text(), 0);
    lv_obj_align(s_clock_label, LV_ALIGN_TOP_RIGHT, -5, 5);

    /* 页面容器（全屏，承载当前页面） */
    s_page_container = lv_obj_create(s_sys_container);
    lv_obj_remove_style_all(s_page_container);
    lv_obj_set_size(s_page_container, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_page_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(s_page_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 显示初始页面 */
    ui_manager_switch_page(UI_PAGE_HOME);

    lvgl_port_unlock();

    ESP_LOGI(TAG, "UI manager initialized (layered architecture)");
}

lv_obj_t *ui_manager_get_page_container(void)
{
    return s_page_container;
}

ui_page_id_t ui_manager_get_current_page(void)
{
    return s_current_page;
}
