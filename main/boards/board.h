/**
 * @file board.h
 * @brief 板卡抽象层头文件
 * @author mkk
 * @date 2026-05-30
 * @note 定义板卡配置结构体和单例接口，
 *       每个板卡在 boards/<name>/board.c 中提供具体配置
 */

#ifndef __BOARD_H__
#define __BOARD_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 板卡配置结构体
 * @note 包含该板卡的所有硬件引脚和参数定义，
 *       各板卡通过填充此结构体来适配不同硬件
 */
typedef struct {
    /* 板卡信息 */
    const char *name;               /* 板卡名称 */

    /* LCD 引脚 */
    int pin_sck;                     /* SPI 时钟引脚 */
    int pin_mosi;                    /* SPI MOSI 引脚 */
    int pin_cs;                      /* SPI 片选引脚 */
    int pin_dc;                      /* 数据/命令引脚 */
    int pin_rst;                     /* 复位引脚 */
    int pin_backlight;               /* 背光控制引脚 */

    /* LCD 参数 */
    int width;                       /* 屏幕宽度（像素） */
    int height;                      /* 屏幕高度（像素） */
    int spi_host;                    /* SPI 主机编号（SPI2_HOST / SPI3_HOST） */
    int spi_mode;                    /* SPI 模式（0-3） */
    int pixel_clock_hz;              /* 像素时钟频率（Hz） */
    bool invert_color;               /* 是否反色显示 */

    /* WiFi 配网参数 */
    const char *ap_ssid;             /* 配网热点名称 */
    const char *ap_password;         /* 配网热点密码 */
    int ap_channel;                  /* WiFi 信道 */
    int ap_max_conn;                 /* 最大连接数 */
} board_t;

/**
 * @brief 获取当前板卡配置（单例）
 * @return 指向板卡配置结构体的常量指针
 * @note 由 boards/<name>/board.c 实现，编译时通过 Kconfig 选择
 */
const board_t *board_get_instance(void);

#endif /* __BOARD_H__ */
