/**
 * @file ui_home.c
 * @brief 首页UI实现
 * @author mkk
 * @date 2026-05-31
 * @note 当前为简洁占位页面，显示项目名称和时钟，
 *       后续替换为桌面女友主界面
 */

#include "ui_home.h"
#include "app_font.h"
#include "lvgl.h"

/* 页面对象引用（用于 destroy 时清理） */
static lv_obj_t *s_label = NULL;
static lv_obj_t *s_clock_label = NULL;

/**
 * @brief 创建首页UI
 */
void ui_home_create(void)
{
    /* 项目名称（居中） */
    s_label = lv_label_create(lv_screen_active());
    lv_label_set_text(s_label, "Desktop Girlfriend");
    lv_obj_set_style_text_font(s_label, app_font_get_text(), 0);
    lv_obj_center(s_label);

    /* 时钟标签（右上角） */
    s_clock_label = lv_label_create(lv_screen_active());
    lv_label_set_text(s_clock_label, "--:--");
    lv_obj_set_style_text_font(s_clock_label, app_font_get_text(), 0);
    lv_obj_align(s_clock_label, LV_ALIGN_TOP_RIGHT, -5, 5);
}

/**
 * @brief 销毁首页UI
 */
void ui_home_destroy(void)
{
    if (s_label) {
        lv_obj_delete(s_label);
        s_label = NULL;
    }
    if (s_clock_label) {
        lv_obj_delete(s_clock_label);
        s_clock_label = NULL;
    }
}

/**
 * @brief 更新首页时钟显示
 * @param time_str 时间字符串（如 "12:30"）
 */
void ui_home_update_clock(const char *time_str)
{
    if (s_clock_label) {
        lv_label_set_text(s_clock_label, time_str);
    }
}
