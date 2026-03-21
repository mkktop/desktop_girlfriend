#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "app_lvgl.h"

#define LVGL_TASK_NAME       "lvgl_task"
#define LVGL_TASK_STACK      4096*2
#define LVGL_TASK_PRIORITY   5
#define LVGL_TASK_CORE       0

static esp_timer_handle_t lvgl_tick_handle = NULL;

static void lvgl_tick_task(void *arg)
{
    (void)arg;
    lv_tick_inc(1);
}

void lvgl_tick_init(void)
{
    esp_timer_create_args_t lvgl_tick_args = {
        .callback = lvgl_tick_task,      /* 定时器回调函数 */
        .arg = NULL,                      /* 回调函数参数 */
        .dispatch_method = ESP_TIMER_TASK, /* 在任务上下文中执行回调 */
        .name = "lvgl_tick"              /* 定时器名称 */
    };
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_args, &lvgl_tick_handle));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_handle, 1000));  /* 周期 1ms (1000us) */
}

static void lvgl_task(void *arg)
{
    (void)arg;
    
    /* 1. 初始化 LVGL 核心库 */
    lv_init();
    
    /* 2. 初始化 LVGL 定时器（提供时间基准） */
    lvgl_tick_init();
    
    /* 3. 初始化显示驱动（ST7789 LCD） */
    lv_port_disp_init();
    
    /* 4. 测试UI组件 */
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello LVGL!");
    lv_obj_center(label);
    
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 40);
    
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Button");
    lv_obj_center(btn_label);
    
    printf("LVGL initialized in task\n");
    
    /* 5. LVGL 任务循环：持续处理 LVGL 任务 */
    while (1) {
        /* 处理 LVGL 的所有任务（渲染、动画、事件等） */
        uint32_t wait_ms = lv_task_handler();
        if (wait_ms < 1) {
            wait_ms = 1;
        }
        if (wait_ms > 10) {
            wait_ms = 10;
        }

        /* 延时 wait_ms，释放 CPU 给其他任务 */
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
}

void app_lvgl_init(void)
{
    xTaskCreatePinnedToCore(
        lvgl_task,
        LVGL_TASK_NAME,
        LVGL_TASK_STACK,
        NULL,
        LVGL_TASK_PRIORITY,
        NULL,
        LVGL_TASK_CORE
    );
}
