/**
 * @file xl9555.c
 * @brief XL9555 I2C IO 扩展芯片驱动实现
 * @author mkk
 * @date 2026-06-06
 * @note XL9555 是 16 路 I2C IO 扩展芯片，两个 8 位端口（P0、P1）。
 *       引脚编号使用 bitmask，bit 0-7 为 P0 端口，bit 8-15 为 P1 端口。
 *       基于 ESP-IDF I2C 主机 API，支持输入输出双向控制。
 */

#include "xl9555.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#define TAG "xl9555"

/* XL9555 寄存器地址 */
#define XL9555_INPUT_PORT0_REG       0
#define XL9555_INPUT_PORT1_REG       1
#define XL9555_OUTPUT_PORT0_REG      2
#define XL9555_OUTPUT_PORT1_REG      3
#define XL9555_INVERSION_PORT0_REG   4
#define XL9555_INVERSION_PORT1_REG   5
#define XL9555_CONFIG_PORT0_REG      6
#define XL9555_CONFIG_PORT1_REG      7

/* 模块状态 */
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_xl9555_dev = NULL;
static uint16_t s_output_state = 0;      /* 当前输出寄存器缓存 */
static bool s_initialized = false;

/* ====== 内部函数 ====== */

/**
 * @brief 写入 XL9555 寄存器
 * @param reg_addr 寄存器地址
 * @param value 写入值（单字节）
 * @return ESP_OK 成功
 */
static esp_err_t xl9555_write_reg(uint8_t reg_addr, uint8_t value)
{
    uint8_t buf[2] = {reg_addr, value};
    return i2c_master_transmit(s_xl9555_dev, buf, 2, 100);
}

/**
 * @brief 读取 XL9555 寄存器
 * @param reg_addr 寄存器地址
 * @param value 读取值输出
 * @return ESP_OK 成功
 */
static esp_err_t xl9555_read_reg(uint8_t reg_addr, uint8_t *value)
{
    return i2c_master_transmit_receive(s_xl9555_dev, &reg_addr, 1, value, 1, 100);
}

/* ====== 公开接口 ====== */

int xl9555_init(int sda_pin, int scl_pin, uint8_t addr, uint16_t output_mask)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return 0;
    }

    /* 初始化 I2C 主机总线 */
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &s_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2C bus: %s", esp_err_to_name(ret));
        return -1;
    }

    /* 添加 XL9555 设备 */
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };

    ret = i2c_master_bus_add_device(s_i2c_bus, &dev_config, &s_xl9555_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add XL9555 device: %s", esp_err_to_name(ret));
        return -1;
    }

    /* 配置引脚方向：output_mask 中为 0 的位配置为输入（1），为 1 的位配置为输出（0） */
    uint16_t config_val = ~output_mask;  /* XL9555: 0=输出, 1=输入 */
    xl9555_write_reg(XL9555_CONFIG_PORT0_REG, (uint8_t)(config_val & 0xFF));
    xl9555_write_reg(XL9555_CONFIG_PORT1_REG, (uint8_t)(config_val >> 8));

    /* 输出引脚初始全部低电平 */
    s_output_state = 0;
    xl9555_write_reg(XL9555_OUTPUT_PORT0_REG, 0x00);
    xl9555_write_reg(XL9555_OUTPUT_PORT1_REG, 0x00);

    s_initialized = true;
    ESP_LOGI(TAG, "XL9555 initialized (addr=0x%02X, output=0x%04X)", addr, output_mask);
    return 0;
}

void xl9555_pin_write(uint16_t pin, int val)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "Not initialized, ignoring pin write");
        return;
    }

    if (val) {
        s_output_state |= pin;
    } else {
        s_output_state &= ~pin;
    }

    /* 根据 bitmask 范围确定写入 P0 还是 P1 寄存器 */
    if (pin & 0xFF00) {
        xl9555_write_reg(XL9555_OUTPUT_PORT1_REG, (uint8_t)(s_output_state >> 8));
    }
    if (pin & 0x00FF) {
        xl9555_write_reg(XL9555_OUTPUT_PORT0_REG, (uint8_t)(s_output_state & 0xFF));
    }
}

int xl9555_pin_read(uint16_t pin)
{
    if (!s_initialized) return 0;

    uint8_t val = 0;
    if (pin & 0xFF00) {
        xl9555_read_reg(XL9555_INPUT_PORT1_REG, &val);
        return (val & (pin >> 8)) ? 1 : 0;
    } else {
        xl9555_read_reg(XL9555_INPUT_PORT0_REG, &val);
        return (val & pin) ? 1 : 0;
    }
}

bool xl9555_is_initialized(void)
{
    return s_initialized;
}
