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
    int pin_rst;                     /* 复位引脚（-1 表示使用 IO 扩展） */
    int pin_backlight;               /* 背光控制引脚（-1 表示使用 IO 扩展） */

    /* 显示参数 */
    int width;                       /* 屏幕宽度（像素） */
    int height;                      /* 屏幕高度（像素） */
    int spi_host;                    /* SPI 主机编号（SPI2_HOST / SPI3_HOST） */
    int spi_mode;                    /* SPI 模式（0-3） */
    uint32_t pixel_clock_hz;         /* 像素时钟频率（Hz） */
    bool invert_color;               /* 是否反色显示 */

    /* IO 扩展芯片配置（可选，use_io_expander=true 时生效） */
    bool use_io_expander;            /* RST/背光是否通过 IO 扩展芯片控制 */
    uint16_t expander_rst_pin;       /* IO 扩展芯片上的 RST 引脚 bitmask */
    uint16_t expander_bl_pin;        /* IO 扩展芯片上的背光引脚 bitmask */
    uint16_t expander_output_mask;   /* IO 扩展芯片输出引脚掩码（需为输出的引脚置1） */
    int i2c_sda_pin;                 /* IO 扩展芯片 I2C SDA 引脚 */
    int i2c_scl_pin;                 /* IO 扩展芯片 I2C SCL 引脚 */
    uint8_t i2c_addr;                /* IO 扩展芯片 7 位 I2C 地址 */
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
 * @brief 字体配置
 * @note 每个板卡可指定使用的内置字体和运行时 CBin 字体，
 *       实现两层字体策略（内置 fallback + 运行时加载）
 */
typedef struct {
    const char *builtin_text_font;   /* 内置文本字体符号名（如 "font_puhui_basic_16_4"） */
    const char *cbin_text_font;      /* CBin 运行时字体文件名（如 "font_puhui_common_16_4.bin"） */
} font_cfg_t;

/**
 * @brief 音频 I2S 编解码配置
 * @note 描述 I2S 引脚、采样率、编解码芯片参数，
 *       适用于 ES8388、ES8311 等 I2S 音频芯片，
 *       i2s_port 填 -1 表示该板卡无音频硬件
 */
typedef struct {
    /* I2S 数据引脚 */
    int i2s_port;                    /* I2S 端口号（I2S_NUM_0 / I2S_NUM_1，-1 无音频） */
    int pin_mclk;                    /* MCLK 引脚（-1 表示不使用） */
    int pin_bclk;                    /* BCLK（SCK）引脚 */
    int pin_ws;                      /* WS（LRCK）引脚 */
    int pin_dout;                    /* DOUT（ESP32 → Codec DAC，播放）引脚 */
    int pin_din;                     /* DIN（Codec ADC → ESP32，录音）引脚 */

    /* 采样率 */
    int input_sample_rate;           /* ADC 采样率（如 16000） */
    int output_sample_rate;          /* DAC 采样率（如 24000） */

    /* 编解码芯片控制 */
    uint8_t codec_addr;              /* 编解码芯片 I2C 地址 */

    /* 功放使能（GPIO 和 IO 扩展二选一） */
    int pa_pin;                      /* 直接 GPIO 功放引脚（-1 不使用） */
    uint16_t pa_expander_pin;        /* IO 扩展芯片功放引脚 bitmask（0 不使用） */
    bool pa_active_low;              /* 功放极性：true=低电平开启 */
} audio_i2s_cfg_t;

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
    font_cfg_t font;                 /* 字体配置 */
    audio_i2s_cfg_t audio;           /* 音频配置（i2s_port=-1 表示无音频） */

    /* 未来扩展预留：
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
