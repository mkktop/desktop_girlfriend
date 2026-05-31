/**
 * @file ui_manager.c
 * @brief UI页面管理器实现
 * @author mkk
 * @date 2026-05-31
 * @note 管理页面切换和事件响应，
 *       注册为事件观察者，监听 WiFi 状态变化和配网模式事件，
 *       在首页与配网引导页之间切换
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

/* 页面状态 */
typedef enum {
    UI_PAGE_HOME,
    UI_PAGE_WIFI_CONFIG
} ui_page_t;

static ui_page_t s_current_page = UI_PAGE_HOME;

/* WiFi状态标签（显示在页面顶部） */
static lv_obj_t *s_status_label = NULL;

/**
 * @brief 切换到配网引导页
 */
static void show_wifi_config_page(void)
{
    if (s_current_page == UI_PAGE_WIFI_CONFIG) {
        return;
    }
    ui_home_destroy();
    ui_wifi_config_create();
    s_current_page = UI_PAGE_WIFI_CONFIG;
}

/**
 * @brief 切换回首页
 */
static void show_home_page(void)
{
    if (s_current_page == UI_PAGE_HOME) {
        return;
    }
    ui_wifi_config_destroy();
    ui_home_create();
    s_current_page = UI_PAGE_HOME;
}

/**
 * @brief 事件观察者回调，更新UI状态
 * @param event_bits 触发的事件位
 * @param user_ctx   未使用
 */
static void ui_event_handler(EventBits_t event_bits, void *user_ctx)
{
    (void)user_ctx;

    if (!lvgl_port_lock(0)) {
        return;
    }

    /* 配网模式进入/退出 */
    if (event_bits & APP_EVENT_WIFI_CONFIG_ENTER) {
        if (s_status_label) {
            lv_label_set_text(s_status_label, "WiFi: \xe9\x85\x8d\xe7\xbd\x91\xe6\xa8\xa1\xe5\xbc\x8f"); /* 配网模式 */
        }
        show_wifi_config_page();
    }
    if (event_bits & APP_EVENT_WIFI_CONFIG_EXIT) {
        show_home_page();
        /* 状态标签由后续 DISCONNECTED/GOT_IP 事件自然更新 */
    }

    /* WiFi 连接状态 */
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

    /* 时间同步 — 更新首页时钟 */
    if (event_bits & APP_EVENT_TIME_SYNCED) {
        if (s_current_page == UI_PAGE_HOME) {
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            char time_str[16];
            strftime(time_str, sizeof(time_str), "%H:%M", tm_info);
            ui_home_update_clock(time_str);
        }
    }

    lvgl_port_unlock();
}

void ui_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing UI manager...");

    /* 注册为事件观察者，关心 WiFi 状态 + 配网模式 + 时间同步事件 */
    const EventBits_t wifi_bits =
        APP_EVENT_WIFI_CONNECTED | APP_EVENT_WIFI_DISCONNECTED | APP_EVENT_WIFI_GOT_IP
      | APP_EVENT_WIFI_CONFIG_ENTER | APP_EVENT_WIFI_CONFIG_EXIT
      | APP_EVENT_TIME_SYNCED;
    app_event_register_handler(ui_event_handler, wifi_bits, NULL);

    /* 在锁保护下创建UI */
    if (lvgl_port_lock(0)) {
        /* 创建状态栏标签 */
        s_status_label = lv_label_create(lv_screen_active());
        lv_label_set_text(s_status_label, "WiFi: \xe7\xad\x89\xe5\xbe\x85\xe8\xbf\x9e\xe6\x8e\xa5..."); /* 等待连接... */
        lv_obj_set_style_text_font(s_status_label, app_font_get_text(), 0);
        lv_obj_align(s_status_label, LV_ALIGN_TOP_LEFT, 5, 5);

        /* 创建首页 */
        ui_home_create();

        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "UI manager initialized");
}

lv_obj_t *ui_manager_get_active_screen(void)
{
    return lv_screen_active();
}
