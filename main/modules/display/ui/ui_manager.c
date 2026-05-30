/**
 * @file ui_manager.c
 * @brief UI页面管理器实现
 * @author mkk
 * @date 2026-05-30
 * @note 管理页面切换和事件响应，
 *       订阅app_event实现WiFi状态等UI更新
 */

#include "ui_manager.h"
#include "ui_home.h"
#include "app_event.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"

static const char *TAG = "ui_manager";

/* 当前活动屏幕 */
static lv_obj_t *s_active_screen = NULL;

/* WiFi状态标签（显示在页面顶部） */
static lv_obj_t *s_status_label = NULL;

/**
 * @brief 应用事件回调，更新UI状态
 * @param event 事件类型
 * @param data 事件数据
 */
static void ui_event_handler(app_event_t event, void *data)
{
    if (!lvgl_port_lock(0)) {
        return;
    }

    switch (event) {
    case APP_EVENT_WIFI_CONNECTED:
        if (s_status_label) {
            lv_label_set_text(s_status_label, "WiFi: 连接中...");
        }
        break;
    case APP_EVENT_WIFI_GOT_IP:
        if (s_status_label && data) {
            char text[64];
            snprintf(text, sizeof(text), "WiFi: IP %s", (const char *)data);
            lv_label_set_text(s_status_label, text);
        }
        break;
    case APP_EVENT_WIFI_DISCONNECTED:
        if (s_status_label) {
            lv_label_set_text(s_status_label, "WiFi: 未连接");
        }
        break;
    default:
        break;
    }

    lvgl_port_unlock();
}

void ui_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing UI manager...");

    /* 注册事件回调 */
    app_event_register(ui_event_handler);

    /* 在锁保护下创建UI */
    if (lvgl_port_lock(0)) {
        /* 创建状态栏标签 */
        s_status_label = lv_label_create(lv_screen_active());
        lv_label_set_text(s_status_label, "WiFi: 等待连接...");
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
