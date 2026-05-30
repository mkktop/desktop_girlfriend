/**
 * @file main.c
 * @brief 应用入口
 * @author mkk
 * @date 2026-05-30
 * @note 初始化顺序：NVS → 事件系统 → WiFi → 显示硬件 → UI
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "modules/event/app_event.h"
#include "modules/wifi/app_wifi.h"
#include "modules/display/app_display.h"
#include "modules/display/ui/ui_manager.h"
#include "nvs_flash.h"

void app_main(void)
{
    printf("Desktop Girlfriend Start\n");

    /* 1. 初始化 NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    /* 2. 初始化事件系统（最先初始化，其他模块注册回调用） */
    app_event_init();

    /* 3. 初始化 WiFi */
    app_wifi_init();
    app_wifi_start();

    /* 4. 初始化显示硬件 */
    app_display_init();

    /* 5. 初始化 UI（在显示硬件之后） */
    ui_manager_init();

    /* 主线程空闲循环 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
