/**
 * @file board.c
 * @brief desktop_girlfriend_V1 板卡配置
 * @author mkk
 * @date 2026-05-30
 * @note ESP32-S3 + ST7789 LCD (240x320) + WiFi 配网
 */

#include "board.h"
#include "driver/spi_common.h"

static const board_t s_board = {
    .name = "desktop_girlfriend_V1",

    /* LCD 引脚 */
    .pin_sck = 9,
    .pin_mosi = 10,
    .pin_cs = 12,
    .pin_dc = 7,
    .pin_rst = 8,
    .pin_backlight = 11,

    /* LCD 参数 */
    .width = 240,
    .height = 320,
    .spi_host = SPI2_HOST,
    .spi_mode = 0,
    .pixel_clock_hz = 80 * 1000 * 1000,
    .invert_color = true,

    /* WiFi 配网参数 */
    .ap_ssid = "mkk_wifi",
    .ap_password = "123456789",
    .ap_channel = 1,
    .ap_max_conn = 1,
};

const board_t *board_get_instance(void)
{
    return &s_board;
}
