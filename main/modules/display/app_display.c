/**
 * @file app_display.c
 * @brief 显示硬件初始化模块实现
 * @author mkk
 * @date 2026-05-30
 * @note 使用ESP-IDF官方esp_lcd框架驱动ST7789 LCD，
 *       通过esp_lvgl_port组件托管LVGL任务、tick和DMA传输，
 *       本文件只负责硬件初始化，不包含任何UI创建逻辑
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_psram.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "lvgl.h"
#include "misc/cache/instance/lv_image_cache.h"
#include "esp_lvgl_port.h"
#include "app_display.h"

static const char *TAG = "app_display";

/* LCD引脚定义 */
#define LCD_SCK_PIN     GPIO_NUM_9
#define LCD_MOSI_PIN    GPIO_NUM_10
#define LCD_CS_PIN      GPIO_NUM_12
#define LCD_DC_PIN      GPIO_NUM_7
#define LCD_RST_PIN     GPIO_NUM_8
#define LCD_BACKLIGHT_PIN GPIO_NUM_11

/* LCD参数 */
#define LCD_WIDTH       240
#define LCD_HEIGHT      320
#define LCD_PIXEL_CLOCK_HZ  (80 * 1000 * 1000)
#define LCD_SPI_MODE    0
#define LCD_SPI_HOST    SPI2_HOST

/* LVGL显示对象 */
static lv_display_t *s_display = NULL;

/**
 * @brief 初始化显示硬件
 */
void app_display_init(void)
{
    ESP_LOGI(TAG, "Initializing display...");

    /* 1. 初始化SPI总线 */
    spi_bus_config_t buscfg = {
        .mosi_io_num = LCD_MOSI_PIN,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = LCD_SCK_PIN,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI bus initialized");

    /* 2. 创建LCD面板SPI IO */
    esp_lcd_panel_io_handle_t panel_io = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = LCD_CS_PIN,
        .dc_gpio_num = LCD_DC_PIN,
        .spi_mode = LCD_SPI_MODE,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_config, &panel_io));
    ESP_LOGI(TAG, "Panel IO created");

    /* 3. 创建ST7789 LCD面板（ESP-IDF内置驱动） */
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
    ESP_LOGI(TAG, "ST7789 panel created");

    /* 4. 初始化LCD面板 */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
    ESP_LOGI(TAG, "LCD panel initialized");

    /* 5. 清屏为白色（避免上电瞬间花屏） */
    uint16_t *line_buf = heap_caps_malloc(LCD_WIDTH * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (line_buf) {
        memset(line_buf, 0xFF, LCD_WIDTH * sizeof(uint16_t));
        for (int y = 0; y < LCD_HEIGHT; y++) {
            esp_lcd_panel_draw_bitmap(panel, 0, y, LCD_WIDTH, y + 1, line_buf);
        }
        free(line_buf);
    }

    /* 6. 开启显示和背光 */
    esp_lcd_panel_disp_on_off(panel, true);
    gpio_set_direction(LCD_BACKLIGHT_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_BACKLIGHT_PIN, 1);

    /* 7. 初始化LVGL核心库 */
    lv_init();
    ESP_LOGI(TAG, "LVGL initialized");

    /* 8. PSRAM图像缓存优化（如有足够PSRAM则启用） */
    size_t psram_size = esp_psram_get_size();
    if (psram_size >= 8 * 1024 * 1024) {
        lv_image_cache_resize(2 * 1024 * 1024, true);
        ESP_LOGI(TAG, "PSRAM image cache enabled (2MB, PSRAM: %u bytes)", psram_size);
    } else if (psram_size >= 2 * 1024 * 1024) {
        lv_image_cache_resize(512 * 1024, true);
        ESP_LOGI(TAG, "PSRAM image cache enabled (512KB, PSRAM: %u bytes)", psram_size);
    }

    /* 9. 初始化LVGL端口（自动创建任务、tick、锁） */
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.task_stack = 6144;
    port_cfg.task_affinity = 1;     /* 绑定到Core 1 */
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));
    ESP_LOGI(TAG, "LVGL port initialized");

    /* 10. 添加显示设备到LVGL端口 */
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io,
        .panel_handle = panel,
        .control_handle = NULL,
        .buffer_size = LCD_WIDTH * 20,
        .double_buffer = false,
        .trans_size = 0,
        .hres = LCD_WIDTH,
        .vres = LCD_HEIGHT,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .swap_bytes = 1,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };
    s_display = lvgl_port_add_disp(&display_cfg);
    ESP_ERROR_CHECK(s_display == NULL ? ESP_FAIL : ESP_OK);
    ESP_LOGI(TAG, "Display initialization complete");
}

lv_display_t *app_display_get(void)
{
    return s_display;
}
