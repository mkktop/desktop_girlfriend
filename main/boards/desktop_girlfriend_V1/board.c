/**
 * @file board.c
 * @brief desktop_girlfriend_V1 板卡配置
 * @author mkk
 * @date 2026-05-30
 * @note ESP32-S3 + ST7789 LCD (240×320) + WiFi 配网
 */

#include "board.h"
#include "driver/spi_common.h"

static const board_t s_board = {
    .name = "desktop_girlfriend_V1",

    /* LCD 显示配置 */
    .lcd = {
        .pin_sck = 9,
        .pin_mosi = 10,
        .pin_cs = 12,
        .pin_dc = 7,
        .pin_rst = 8,
        .pin_backlight = 11,
        .width = 240,
        .height = 320,
        .spi_host = SPI2_HOST,
        .spi_mode = 0,
        .pixel_clock_hz = 80 * 1000 * 1000,
        .invert_color = true,
        .use_io_expander = false,
    },

    /* WiFi 配网参数 */
    .wifi_ap = {
        .ssid_prefix = "desktop_girlfriend",
        .password = "123456789",
        .channel = 1,
        .max_conn = 1,
    },

    /* 字体配置（阿里巴巴普惠体，与小智一致） */
    .font = {
        .builtin_text_font = "font_puhui_basic_16_4",
        .cbin_text_font = "font_puhui_common_16_4.bin",
    },
};

const board_t *board_get_instance(void)
{
    return &s_board;
}
