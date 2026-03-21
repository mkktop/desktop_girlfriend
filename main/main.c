#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_lvgl.h"

void app_main(void)
{
    printf("LVGL Test Start\n");
    
    /* 初始化 LVGL */
    app_lvgl_init();
    
    /* 主线程空闲循环 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
