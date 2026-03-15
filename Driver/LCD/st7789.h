/**
 * @file st7789.h
 * @brief ST7789V LCD驱动头文件
 * @author mkk
 * @date 2026-03-8
 * @note 仅保留LVGL必需的函数
 */

#ifndef __ST7789_H__
#define __ST7789_H__

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_err.h"

/* 引脚定义 */
#define ST7789_DC_PIN   GPIO_NUM_7
#define ST7789_RST_PIN  GPIO_NUM_8
#define ST7789_SCK_PIN  GPIO_NUM_9
#define ST7789_SDA_PIN  GPIO_NUM_10
#define ST7789_PWR_PIN  GPIO_NUM_11
#define ST7789_CS_PIN   GPIO_NUM_12

/* 屏幕分辨率 */
#define ST7789_WIDTH    240
#define ST7789_HEIGHT   320

/**
 * @brief 屏幕旋转方向枚举
 */
typedef enum {
    ST7789_ROTATION_0 = 0,
    ST7789_ROTATION_90,
    ST7789_ROTATION_180,
    ST7789_ROTATION_270
} st7789_rotation_t;

/**
 * @brief 初始化ST7789 LCD
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t st7789_init(void);

/**
 * @brief 设置屏幕旋转方向
 * @param rotation 旋转方向
 */
void st7789_set_rotation(st7789_rotation_t rotation);

/**
 * @brief 在指定区域绘制位图（LVGL专用）
 * @param x1 起始X坐标
 * @param y1 起始Y坐标
 * @param x2 结束X坐标
 * @param y2 结束Y坐标
 * @param data 位图数据(RGB565格式，大端序)
 */
void st7789_draw_bitmap(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t *data);

/**
 * @brief 设置背光开关
 * @param on true-开启，false-关闭
 */
void st7789_set_backlight(bool on);

#endif /* __ST7789_H__ */
