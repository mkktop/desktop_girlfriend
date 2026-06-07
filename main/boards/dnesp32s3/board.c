/**
 * @file board.c
 * @brief DNESP32S3 板卡配置（正点原子 ESP32-S3 开发板）
 * @author mkk
 * @date 2026-06-06
 * @note ATK-MWS3S 模组，16MB Flash + 8MB PSRAM
 *       SPI LCD (ST7789V, 240×320) RST/背光通过 XL9555 IO 扩展芯片控制
 */

#include "board.h"
#include "driver/spi_common.h"
#include "driver/i2s_std.h"
#include "es8388_codec.h"

/* XL9555 IO 扩展引脚定义（bitmask） */
#define XL9555_SPK_EN_PIN      0x0004   /* 功放 MD8002A 使能引脚 P02，低电平有效 */
#define XL9555_SLCD_RST_PIN    0x0400   /* SPI_LCD 复位引脚 P12 */
#define XL9555_SLCD_PWR_PIN    0x0800   /* SPI_LCD 背光引脚 P13 */

static const board_t s_board = {
    .name = "DNESP32S3",

    /* LCD 显示配置（2.4寸 ATK-MD0240, ST7789V, 240×320） */
    .lcd = {
        .pin_sck = 12,
        .pin_mosi = 11,
        .pin_cs = 21,
        .pin_dc = 40,
        .pin_rst = -1,                      /* 不直接使用 GPIO */
        .pin_backlight = -1,                /* 不直接使用 GPIO */
        .width = 240,
        .height = 320,
        .spi_host = SPI2_HOST,
        .spi_mode = 0,
        .pixel_clock_hz = 80 * 1000 * 1000,
        .invert_color = true,

        /* IO 扩展芯片配置 */
        .use_io_expander = true,
        .expander_rst_pin = XL9555_SLCD_RST_PIN,
        .expander_bl_pin = XL9555_SLCD_PWR_PIN,
        .expander_output_mask = XL9555_SPK_EN_PIN | XL9555_SLCD_RST_PIN | XL9555_SLCD_PWR_PIN,
        .i2c_sda_pin = 41,
        .i2c_scl_pin = 42,
        .i2c_addr = 0x20,
    },

    /* WiFi 配网参数 */
    .wifi_ap = {
        .ssid_prefix = "desktop_girlfriend",
        .password = "123456789",
        .channel = 1,
        .max_conn = 1,
    },

    /* 字体配置 */
    .font = {
        .builtin_text_font = "font_puhui_basic_16_4",
        .cbin_text_font = "font_puhui_common_16_4.bin",
    },

    /* 音频配置（ES8388 编解码芯片 + MD8002A 功放） */
    .audio = {
        .i2s_port = I2S_NUM_0,
        .pin_mclk = 3,
        .pin_bclk = 46,
        .pin_ws = 9,
        .pin_dout = 10,                     /* ESP32 → ES8388 DAC（播放） */
        .pin_din = 14,                      /* ES8388 ADC → ESP32（录音） */
        .input_sample_rate = 24000,
        .output_sample_rate = 24000,        /* 全双工使用统一采样率 */
        .codec_addr = ES8388_CODEC_DEFAULT_ADDR,
        .pa_pin = -1,                       /* 不使用直接 GPIO */
        .pa_expander_pin = 0x0004,          /* XL9555 P02，低电平有效 */
        .pa_active_low = true,
    },
};

const board_t *board_get_instance(void)
{
    return &s_board;
}
