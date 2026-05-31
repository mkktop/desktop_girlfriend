/**
 * @file board.h
 * @brief 板卡抽象层头文件
 * @author mkk
 * @date 2026-05-30
 * @note 定义板卡配置结构体和单例接口，
 *       每个板卡在 boards/<name>/board.c 中提供具体配置，
 *       外设配置通过嵌套结构体分组，便于未来扩展
 */

#ifndef __BOARD_H__
#define __BOARD_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief LCD 显示配置
 * @note SPI 接口 LCD 的引脚和参数，
 *       适用于 ST7789、ILI9341、GC9A01 等 SPI 屏
 */
typedef struct {
    /* SPI 引脚 */
    int pin_sck;                     /* SPI 时钟引脚 */
    int pin_mosi;                    /* SPI MOSI 引脚 */
    int pin_cs;                      /* SPI 片选引脚 */
    int pin_dc;                      /* 数据/命令引脚 */
    int pin_rst;                     /* 复位引脚 */
    int pin_backlight;               /* 背光控制引脚 */

    /* 显示参数 */
    int width;                       /* 屏幕宽度（像素） */
    int height;                      /* 屏幕高度（像素） */
    int spi_host;                    /* SPI 主机编号（SPI2_HOST / SPI3_HOST） */
    int spi_mode;                    /* SPI 模式（0-3） */
    uint32_t pixel_clock_hz;         /* 像素时钟频率（Hz） */
    bool invert_color;               /* 是否反色显示 */
} lcd_cfg_t;

/**
 * @brief WiFi 配网 AP 参数
 */
typedef struct {
    const char *ssid_prefix;         /* 配网热点名称前缀（运行时拼接MAC后4位） */
    const char *password;            /* 配网热点密码 */
    int channel;                     /* WiFi 信道 */
    int max_conn;                    /* 最大连接数 */
} wifi_ap_cfg_t;

/**
 * @brief 板卡配置结构体
 * @note 各板卡通过填充此结构体来适配不同硬件，
 *       外设配置通过嵌套结构体分组，扩展时只需添加新的 cfg_t
 */
typedef struct {
    /* 板卡信息 */
    const char *name;                /* 板卡名称 */

    /* 外设配置 */
    lcd_cfg_t lcd;                   /* LCD 显示配置 */
    wifi_ap_cfg_t wifi_ap;           /* WiFi 配网参数 */

    /* 未来扩展预留：
     * audio_i2s_cfg_t audio;        // 音频 I2S 配置
     * touch_cfg_t touch;            // 触摸屏配置
     * button_cfg_t button;          // 按钮配置
     * power_cfg_t power;            // 电源管理配置
     */
} board_t;

/**
 * @brief 获取当前板卡配置（单例）
 * @return 指向板卡配置结构体的常量指针
 * @note 由 boards/<name>/board.c 实现，编译时通过 Kconfig 选择
 */
const board_t *board_get_instance(void);

#endif /* __BOARD_H__ */
