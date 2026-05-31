/**
 * @file ui_home.c
 * @brief 首页UI实现
 * @author mkk
 * @date 2026-05-31
 * @note 当前为简洁占位页面，显示项目名称，
 *       后续替换为桌面女友主界面
 */

#include "ui_home.h"
#include "app_font.h"
#include "lvgl.h"

/* 页面对象引用 */
static lv_obj_t *s_label = NULL;

/**
 * @brief 创建首页UI
 * @param parent 页面容器
 */
void ui_home_create(lv_obj_t *parent)
{
    s_label = lv_label_create(parent);
    lv_label_set_text(s_label, "Desktop Girlfriend");
    lv_obj_set_style_text_font(s_label, app_font_get_text(), 0);
    lv_obj_center(s_label);
}

/**
 * @brief 首页事件处理
 * @param event_bits 当前触发的事件位
 */
void ui_home_on_event(EventBits_t event_bits)
{
    /* 首页特有的事件处理（预留） */
    (void)event_bits;
}
