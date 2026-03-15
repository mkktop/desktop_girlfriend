/**
 * @file st7789.c
 * @brief ST7789V LCD驱动实现文件
 * @author mkk
 * @date 2026-03-8
 * @note 使用硬件SPI驱动，适用于ESP32-S3
 *       此版本为LVGL精简版，仅保留LVGL必需的函数
 */

#include "st7789.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ST7789";    /* 日志标签 */

static spi_device_handle_t spi_handle;           /* SPI设备句柄 */
static uint16_t st7789_width = ST7789_WIDTH;     /* 当前屏幕宽度 */
static uint16_t st7789_height = ST7789_HEIGHT;   /* 当前屏幕高度 */

#define FILL_BUF_SIZE 1024           /* 填充缓冲区大小(像素数) */
static uint16_t *fill_buf = NULL;    /* 填充缓冲区指针 */

/**
 * @brief 硬件复位LCD
 * @note 通过RST引脚进行硬件复位
 */
static void st7789_reset(void)
{
    /* 拉高RST，等待100ms */
    gpio_set_level(ST7789_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    /* 拉低RST，等待100ms */
    gpio_set_level(ST7789_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    /* 拉高RST，等待100ms，完成复位 */
    gpio_set_level(ST7789_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
}

/**
 * @brief 写命令到LCD
 * @param cmd 命令字节
 * @note DC引脚拉低表示发送命令
 */
static void st7789_write_cmd(uint8_t cmd)
{
    gpio_set_level(ST7789_DC_PIN, 0);    /* DC=0，发送命令 */
    spi_transaction_t t = {
        .length = 8,                     /* 8位数据 */
        .tx_buffer = &cmd,               /* 命令数据 */
    };
    spi_device_polling_transmit(spi_handle, &t);    /* 轮询发送 */
}

/**
 * @brief 写数据到LCD
 * @param data 数据指针
 * @param len 数据长度(字节)
 * @note DC引脚拉高表示发送数据
 */
static void st7789_write_data(uint8_t *data, uint32_t len)
{
    gpio_set_level(ST7789_DC_PIN, 1);    /* DC=1，发送数据 */
    spi_transaction_t t = {
        .length = len * 8,               /* 数据位数 */
        .tx_buffer = data,               /* 数据指针 */
    };
    spi_device_polling_transmit(spi_handle, &t);    /* 轮询发送 */
}

/**
 * @brief 写单字节数据到LCD
 * @param data 数据字节
 */
static void st7789_write_data_byte(uint8_t data)
{
    gpio_set_level(ST7789_DC_PIN, 1);    /* DC=1，发送数据 */
    spi_transaction_t t = {
        .length = 8,                     /* 8位数据 */
        .tx_buffer = &data,              /* 数据字节 */
    };
    spi_device_polling_transmit(spi_handle, &t);    /* 轮询发送 */
}

/**
 * @brief 写双字节数据到LCD(大端序)
 * @param data 16位数据
 */
static void st7789_write_data_word(uint16_t data)
{
    uint8_t buf[2] = {data >> 8, data & 0xFF};    /* 高字节在前 */
    st7789_write_data(buf, 2);
}

/**
 * @brief 设置显示窗口
 * @param x1 起始X坐标
 * @param y1 起始Y坐标
 * @param x2 结束X坐标
 * @param y2 结束Y坐标
 * @note 发送CASET(0x2A)和RASET(0x2B)命令设置窗口
 */
static void st7789_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    /* 设置列地址(X方向) */
    st7789_write_cmd(0x2A);
    st7789_write_data_word(x1);
    st7789_write_data_word(x2);
    
    /* 设置行地址(Y方向) */
    st7789_write_cmd(0x2B);
    st7789_write_data_word(y1);
    st7789_write_data_word(y2);
    
    /* 开始写显存 */
    st7789_write_cmd(0x2C);
}

/**
 * @brief 发送初始化命令序列
 * @note 包含LCD的各种参数配置
 */
static void st7789_init_sequence(void)
{
    /* 退出睡眠模式 */
    st7789_write_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    
    /* 设置显示方向 */
    st7789_write_cmd(0x36);
    st7789_write_data_byte(0x00);
    
    /* 设置像素格式: 16位/像素 */
    st7789_write_cmd(0x3A);
    st7789_write_data_byte(0x05);
    
    /* 设置门控控制参数 */
    st7789_write_cmd(0xB2);
    uint8_t b2_data[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    st7789_write_data(b2_data, 5);
    
    /* 设置VCOM设置 */
    st7789_write_cmd(0xB7);
    st7789_write_data_byte(0x35);
    
    /* 设置VCOM电压 */
    st7789_write_cmd(0xBB);
    st7789_write_data_byte(0x19);
    
    /* 设置LCM控制 */
    st7789_write_cmd(0xC0);
    st7789_write_data_byte(0x2C);
    
    /* 设置VDV和VRH命令使能 */
    st7789_write_cmd(0xC2);
    st7789_write_data_byte(0x01);
    
    /* 设置VRH值 */
    st7789_write_cmd(0xC3);
    st7789_write_data_byte(0x12);
    
    /* 设置VDV值 */
    st7789_write_cmd(0xC4);
    st7789_write_data_byte(0x20);
    
    /* 设置帧率: 60Hz */
    st7789_write_cmd(0xC6);
    st7789_write_data_byte(0x0F);
    
    /* 设置电源控制 */
    st7789_write_cmd(0xD0);
    uint8_t d0_data[] = {0xA4, 0xA1};
    st7789_write_data(d0_data, 2);
    
    /* 正向伽马校正 */
    st7789_write_cmd(0xE0);
    uint8_t e0_data[] = {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23};
    st7789_write_data(e0_data, 14);
    
    /* 负向伽马校正 */
    st7789_write_cmd(0xE1);
    uint8_t e1_data[] = {0xD0, 0x04, 0x0C, 0x11, 0x12, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23};
    st7789_write_data(e1_data, 14);
    
    /* 开启反色显示 */
    st7789_write_cmd(0x21);
    
    /* 开启显示 */
    st7789_write_cmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(20));
}

/**
 * @brief 初始化ST7789 LCD
 * @return ESP_OK 成功，其他值失败
 * @note 初始化SPI总线、GPIO、分配DMA缓冲区、复位并配置LCD
 */
esp_err_t st7789_init(void)
{
    esp_err_t ret;
    
    /* SPI总线配置 */
    spi_bus_config_t buscfg = {
        .mosi_io_num = ST7789_SDA_PIN,              /* MOSI引脚 */
        .miso_io_num = -1,                          /* 不使用MISO */
        .sclk_io_num = ST7789_SCK_PIN,              /* 时钟引脚 */
        .quadwp_io_num = -1,                        /* 不使用四线模式 */
        .quadhd_io_num = -1,                        /* 不使用四线模式 */
        .max_transfer_sz = ST7789_WIDTH * ST7789_HEIGHT * 2,    /* 最大传输大小 */
    };
    
    /* SPI设备配置 */
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 80 * 1000 * 1000,         /* 80MHz时钟 */
        .mode = 0,                                   /* SPI模式0 */
        .spics_io_num = ST7789_CS_PIN,              /* 片选引脚 */
        .queue_size = 7,                             /* 事务队列大小 */
    };
    
    /* 初始化SPI总线 */
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    
    /* 添加SPI设备 */
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return ret;
    }
    
    /* 配置GPIO引脚(DC、RST、PWR) */
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,             /* 禁用中断 */
        .mode = GPIO_MODE_OUTPUT,                   /* 输出模式 */
        .pin_bit_mask = (1ULL << ST7789_DC_PIN) | (1ULL << ST7789_RST_PIN) | (1ULL << ST7789_PWR_PIN),
        .pull_down_en = 0,                          /* 禁用下拉 */
        .pull_up_en = 0,                            /* 禁用上拉 */
    };
    gpio_config(&io_conf);
    
    /* 分配DMA填充缓冲区 */
    fill_buf = heap_caps_malloc(FILL_BUF_SIZE * 2, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (fill_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate fill buffer");
        return ESP_ERR_NO_MEM;
    }
    
    /* 开启背光 */
    gpio_set_level(ST7789_PWR_PIN, 1);
    
    /* 硬件复位 */
    st7789_reset();
    
    /* 发送初始化序列 */
    st7789_init_sequence();
    
    ESP_LOGI(TAG, "ST7789 initialized successfully");
    return ESP_OK;
}

/**
 * @brief 设置屏幕旋转方向
 * @param rotation 旋转方向
 * @note 根据旋转方向更新屏幕宽高
 */
void st7789_set_rotation(st7789_rotation_t rotation)
{
    /* 发送内存访问控制命令 */
    st7789_write_cmd(0x36);
    
    switch (rotation) {
        case ST7789_ROTATION_0:        /* 正常方向 */
            st7789_write_data_byte(0x00);
            st7789_width = ST7789_WIDTH;
            st7789_height = ST7789_HEIGHT;
            break;
        case ST7789_ROTATION_90:       /* 顺时针旋转90度 */
            st7789_write_data_byte(0x60);
            st7789_width = ST7789_HEIGHT;
            st7789_height = ST7789_WIDTH;
            break;
        case ST7789_ROTATION_180:      /* 旋转180度 */
            st7789_write_data_byte(0xC0);
            st7789_width = ST7789_WIDTH;
            st7789_height = ST7789_HEIGHT;
            break;
        case ST7789_ROTATION_270:      /* 顺时针旋转270度 */
            st7789_write_data_byte(0xA0);
            st7789_width = ST7789_HEIGHT;
            st7789_height = ST7789_WIDTH;
            break;
    }
}

/**
 * @brief 在指定区域绘制位图
 * @param x1 起始X坐标
 * @param y1 起始Y坐标
 * @param x2 结束X坐标
 * @param y2 结束Y坐标
 * @param data 位图数据(RGB565格式，大端序)
 * @note 用于LVGL显示刷新
 */
void st7789_draw_bitmap(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t *data)
{
    uint32_t size = (x2 - x1 + 1) * (y2 - y1 + 1) * 2;
    
    st7789_set_window(x1, y1, x2, y2);
    
    gpio_set_level(ST7789_DC_PIN, 1);
    
    while (size > 0) {
        uint32_t send_size = (size > (FILL_BUF_SIZE * 2)) ? (FILL_BUF_SIZE * 2) : size;
        
        spi_transaction_t t = {
            .length = send_size * 8,
            .tx_buffer = data,
        };
        spi_device_polling_transmit(spi_handle, &t);
        
        data += send_size;
        size -= send_size;
    }
}

/**
 * @brief 设置背光开关
 * @param on true-开启，false-关闭
 */
void st7789_set_backlight(bool on)
{
    gpio_set_level(ST7789_PWR_PIN, on ? 1 : 0);
}
