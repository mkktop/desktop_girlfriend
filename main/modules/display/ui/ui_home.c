/**
 * @file ui_home.c
 * @brief 首页UI实现
 * @author mkk
 * @date 2026-05-30
 * @note 当前为测试UI，后续替换为桌面女友主界面
 */

#include "ui_home.h"
#include "lvgl.h"

/**
 * @brief 创建首页UI
 */
void ui_home_create(void)
{
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello LVGL!");
    lv_obj_center(label);

    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 40);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Button");
    lv_obj_center(btn_label);
}
