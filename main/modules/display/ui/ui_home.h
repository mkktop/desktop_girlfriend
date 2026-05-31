/**
 * @file ui_home.h
 * @brief 首页UI头文件
 * @author mkk
 * @date 2026-05-31
 * @note 首页为简洁占位，后续替换为桌面女友主界面
 */

#ifndef __UI_HOME_H__
#define __UI_HOME_H__

/**
 * @brief 创建首页UI
 * @note 必须在 lvgl_port_lock() 保护下调用
 */
void ui_home_create(void);

/**
 * @brief 销毁首页UI
 * @note 必须在 lvgl_port_lock() 保护下调用
 */
void ui_home_destroy(void);

/**
 * @brief 更新首页时钟显示
 * @param time_str 时间字符串（如 "12:30"）
 * @note 必须在 lvgl_port_lock() 保护下调用
 */
void ui_home_update_clock(const char *time_str);

#endif /* __UI_HOME_H__ */
