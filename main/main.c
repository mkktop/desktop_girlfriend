/**
 * @file main.c
 * @brief 应用入口
 * @author mkk
 * @date 2026-05-30
 * @note 初始化顺序：NVS → 事件系统 → SNTP → WiFi → 显示硬件 → 字体 → UI → 音频，
 *       主线程以优先级10运行 EventGroup 驱动的事件循环，
 *       按优先级顺序处理各类事件
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "modules/event/app_event.h"
#include "modules/wifi/app_wifi.h"
#include "modules/sntp/app_sntp.h"
#include "modules/display/app_display.h"
#include "modules/display/app_font.h"
#include "modules/display/ui/ui_manager.h"
#include "modules/audio/app_audio.h"
#include "nvs_flash.h"
#include "esp_log.h"

#define TAG "main"

void app_main(void)
{
    printf("Desktop Girlfriend Start\n");

    /* 1. 初始化 NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 2. 初始化事件系统（最先初始化，其他模块依赖） */
    app_event_init();

    /* 3. 初始化 SNTP（仅注册事件处理器，实际同步在获取 IP 后） */
    app_sntp_init();

    /* 4. 初始化 WiFi */
    app_wifi_init();
    app_wifi_start();

    /* 5. 初始化显示硬件（LVGL 核心初始化，包含 XL9555 IO 扩展芯片） */
    app_display_init();

    /* 6. 初始化字体管理器（内置字体 + 尝试加载 CBin 运行时字体） */
    app_font_init();

    /* 7. 初始化 UI（在显示硬件和字体之后） */
    ui_manager_init();

    /* 8. 初始化音频（编解码芯片 + 播放启动提示音，依赖 XL9555） */
    app_audio_init();

    /* 9. 提升主任务优先级（确保事件及时响应） */
    vTaskPrioritySet(NULL, 10);
    ESP_LOGI(TAG, "Main task priority set to 10");

    /* 10. 主事件循环（按优先级顺序处理，非 else-if，可同时处理多个位） */
    const EventBits_t all_bits = APP_EVENT_ALL_BITS | APP_EVENT_SCHEDULE_PENDING;
    while (1) {
        EventBits_t bits = app_event_wait(all_bits, true, portMAX_DELAY);

        /* WiFi 事件 */
        if (bits & APP_EVENT_WIFI_CONNECTED) {
            app_event_dispatch_to_handlers(APP_EVENT_WIFI_CONNECTED);
        }
        if (bits & APP_EVENT_WIFI_DISCONNECTED) {
            app_event_dispatch_to_handlers(APP_EVENT_WIFI_DISCONNECTED);
        }
        if (bits & APP_EVENT_WIFI_GOT_IP) {
            app_event_dispatch_to_handlers(APP_EVENT_WIFI_GOT_IP);
        }
        if (bits & APP_EVENT_WIFI_AP_START) {
            app_event_dispatch_to_handlers(APP_EVENT_WIFI_AP_START);
        }
        if (bits & APP_EVENT_WIFI_AP_STOP) {
            app_event_dispatch_to_handlers(APP_EVENT_WIFI_AP_STOP);
        }
        if (bits & APP_EVENT_WIFI_STA_START) {
            app_event_dispatch_to_handlers(APP_EVENT_WIFI_STA_START);
        }
        if (bits & APP_EVENT_WIFI_SCANNING) {
            app_event_dispatch_to_handlers(APP_EVENT_WIFI_SCANNING);
        }
        if (bits & APP_EVENT_WIFI_CONNECTING) {
            app_event_dispatch_to_handlers(APP_EVENT_WIFI_CONNECTING);
        }
        if (bits & APP_EVENT_WIFI_CONFIG_ENTER) {
            app_event_dispatch_to_handlers(APP_EVENT_WIFI_CONFIG_ENTER);
        }
        if (bits & APP_EVENT_WIFI_CONFIG_EXIT) {
            app_event_dispatch_to_handlers(APP_EVENT_WIFI_CONFIG_EXIT);
        }

        /* 时间同步事件 */
        if (bits & APP_EVENT_TIME_SYNCED) {
            app_event_dispatch_to_handlers(APP_EVENT_TIME_SYNCED);
        }

        /* Schedule 延迟回调（最后处理） */
        if (bits & APP_EVENT_SCHEDULE_PENDING) {
            app_event_schedule_drain();
        }
    }
}
