/**
 * @file app_display.h
 * @brief 显示硬件初始化模块头文件
 * @author mkk
 * @date 2026-05-30
 * @note 基于esp_lcd + esp_lvgl_port，只负责硬件初始化，
 *       不包含任何UI创建逻辑
 */

#ifndef __APP_DISPLAY_H__
#define __APP_DISPLAY_H__

#include "lvgl.h"

/**
 * @brief 初始化显示硬件
 * @note 完成SPI总线、ST7789面板、LVGL端口初始化，
 *       esp_lvgl_port自动创建LVGL任务并管理DMA传输
 */
void app_display_init(void);

/**
 * @brief 获取LVGL显示对象
 * @return lv_display_t* 显示对象指针，初始化前返回NULL
 */
lv_display_t *app_display_get(void);

#endif /* __APP_DISPLAY_H__ */
