/**
 * @file ui_home.c
 * @brief 首页UI实现
 * @author mkk
 * @date 2026-05-31
 * @note 当前为简洁占位页面，显示项目名称，
 *       后续替换为桌面女友主界面
 */

#include "ui_home.h"
#include "lvgl.h"

/* 页面对象引用（用于 destroy 时清理） */
static lv_obj_t *s_label = NULL;

/**
 * @brief 创建首页UI
 */
void ui_home_create(void)
{
    s_label = lv_label_create(lv_screen_active());
    lv_label_set_text(s_label, "Desktop Girlfriend");
    lv_obj_center(s_label);
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
}
