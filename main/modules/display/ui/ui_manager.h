/**
 * @file ui_manager.h
 * @brief UI页面管理器头文件
 * @author mkk
 * @date 2026-05-30
 * @note 管理页面切换和事件响应，订阅app_event更新UI状态
 */

#ifndef __UI_MANAGER_H__
#define __UI_MANAGER_H__

#include "lvgl.h"

/**
 * @brief 初始化UI管理器
 * @note 创建初始页面，注册事件回调
 *       必须在app_display_init()之后调用
 */
void ui_manager_init(void);

/**
 * @brief 获取当前活动屏幕
 * @return lv_obj_t* 当前屏幕对象
 */
lv_obj_t *ui_manager_get_active_screen(void);

#endif /* __UI_MANAGER_H__ */
