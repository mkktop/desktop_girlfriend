#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "modules/lvgl/app_lvgl.h"
#include "modules/wifi/bsp_wifi_web.h"
#include "nvs_flash.h"

void app_main(void)
{
    printf("LVGL Test Start\n");
    
    /* 初始化 LVGL */
    app_lvgl_init();
    
    /* 初始化 NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    /* 初始化 WiFi Web 服务器 */
    bsp_wifi_web_init();
    bsp_wifi_web_start();
    
    /* 主线程空闲循环 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
