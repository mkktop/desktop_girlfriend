/**
 * @file ui_manager.c
 * @brief UI页面管理器实现
 * @author mkk
 * @date 2026-05-30
 * @note 管理页面切换和事件响应，
 *       注册为事件观察者，按事件位掩码过滤 WiFi 状态更新
 */

#include "ui_manager.h"
#include "ui_home.h"
#include "app_event.h"
#include "app_wifi.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"

static const char *TAG = "ui_manager";

/* 当前活动屏幕 */
static lv_obj_t *s_active_screen = NULL;

/* WiFi状态标签（显示在页面顶部） */
static lv_obj_t *s_status_label = NULL;

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

    lvgl_port_unlock();
}

void ui_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing UI manager...");

    /* 注册为事件观察者，只关心 WiFi 相关事件 */
    const EventBits_t wifi_bits =
        APP_EVENT_WIFI_CONNECTED | APP_EVENT_WIFI_DISCONNECTED | APP_EVENT_WIFI_GOT_IP;
    app_event_register_handler(ui_event_handler, wifi_bits, NULL);

    /* 在锁保护下创建UI */
    if (lvgl_port_lock(0)) {
        /* 创建状态栏标签 */
        s_status_label = lv_label_create(lv_screen_active());
        lv_label_set_text(s_status_label, "WiFi: \xe7\xad\x89\xe5\xbe\x85\xe8\xbf\x9e\xe6\x8e\xa5..."); /* 等待连接... */
        lv_obj_align(s_status_label, LV_ALIGN_TOP_LEFT, 5, 5);

        /* 创建首页 */
        ui_home_create();

        s_active_screen = lv_screen_active();

        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "UI manager initialized");
}

lv_obj_t *ui_manager_get_active_screen(void)
{
    return s_active_screen;
}
