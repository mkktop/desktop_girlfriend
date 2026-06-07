/**
 * @file app_audio_codec.c
 * @brief 音频编解码芯片抽象层实现
 * @author mkk
 * @date 2026-06-06
 * @note 基于 esp_codec_dev 组件封装 ES8388 初始化和读写操作，
 *       I2S 全双工模式，通过 board config 适配引脚和参数。
 *       配置参考 xiaozhi-esp32 项目的 ES8388 实现，确保兼容性。
 */

#include "app_audio_codec.h"
#include "board.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "xl9555.h"
#include "es8388_codec.h"

#define TAG "audio_codec"

/* DMA 缓冲区配置 */
#define CODEC_DMA_DESC_NUM    6
#define CODEC_DMA_FRAME_NUM   240

/* 模块状态 */
static i2s_chan_handle_t s_tx_handle = NULL;
static i2s_chan_handle_t s_rx_handle = NULL;

static const audio_codec_ctrl_if_t *s_ctrl_if = NULL;
static const audio_codec_gpio_if_t *s_gpio_if = NULL;
static const audio_codec_if_t *s_codec_if = NULL;
static const audio_codec_data_if_t *s_data_if = NULL;

static esp_codec_dev_handle_t s_output_dev = NULL;
static esp_codec_dev_handle_t s_input_dev = NULL;

static bool s_initialized = false;
static bool s_output_opened = false;
static bool s_input_opened = false;
static int s_volume = 100;            /* 默认音量 100% */
static float s_gain = 24.0f;          /* 默认输入增益 24dB */
static int s_pa_pin = -1;             /* GPIO 功放引脚 */
static uint16_t s_pa_expander_pin = 0; /* IO 扩展功放引脚 */
static bool s_pa_active_low = false;  /* 功放极性 */

/* ====== 内部函数 ====== */

/**
 * @brief 创建 I2S 全双工通道（TX + RX）
 * @note 配置完全匹配 xiaozhi-esp32 的 CreateDuplexChannels
 */
static int create_duplex_channels(const audio_i2s_cfg_t *cfg)
{
    /* 创建全双工 I2S 通道 */
    i2s_chan_config_t chan_cfg = {
        .id = cfg->i2s_port,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = CODEC_DMA_DESC_NUM,
        .dma_frame_num = CODEC_DMA_FRAME_NUM,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };

    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx_handle, &s_rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return -1;
    }

    /* 配置 I2S 标准模式（匹配 xiaozhi 的 slot 配置） */
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)cfg->output_sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false,
        },
        .gpio_cfg = {
            .mclk = cfg->pin_mclk >= 0 ? cfg->pin_mclk : GPIO_NUM_NC,
            .bclk = cfg->pin_bclk,
            .ws = cfg->pin_ws,
            .dout = cfg->pin_dout,
            .din = cfg->pin_din,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    /* 初始化 TX 和 RX 通道，立即使能（匹配 xiaozhi 的 CreateDuplexChannels） */
    ret = i2s_channel_init_std_mode(s_tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2S TX: %s", esp_err_to_name(ret));
        return -1;
    }

    ret = i2s_channel_init_std_mode(s_rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2S RX: %s", esp_err_to_name(ret));
        return -1;
    }

    ret = i2s_channel_enable(s_tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S TX: %s", esp_err_to_name(ret));
        return -1;
    }

    ret = i2s_channel_enable(s_rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S RX: %s", esp_err_to_name(ret));
        return -1;
    }

    ESP_LOGI(TAG, "I2S duplex channels created (rate=%d, mclk=%d, bclk=%d, ws=%d)",
             cfg->output_sample_rate, cfg->pin_mclk, cfg->pin_bclk, cfg->pin_ws);
    return 0;
}

/* ====== 公开接口 ====== */

int app_audio_codec_init(i2c_master_bus_handle_t i2c_bus)
{
    const board_t *board = board_get_instance();
    const audio_i2s_cfg_t *cfg = &board->audio;

    /* 无音频硬件的板卡直接跳过 */
    if (cfg->i2s_port < 0) {
        ESP_LOGI(TAG, "No audio hardware on this board, skipping");
        return 0;
    }

    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return 0;
    }

    ESP_LOGI(TAG, "Initializing audio codec for board: %s...", board->name);

    /* 1. 创建 I2S 全双工通道 */
    if (create_duplex_channels(cfg) != 0) {
        return -1;
    }

    /* 2. 创建 I2S 数据接口 */
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = cfg->i2s_port,
        .rx_handle = s_rx_handle,
        .tx_handle = s_tx_handle,
    };
    s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    if (s_data_if == NULL) {
        ESP_LOGE(TAG, "Failed to create I2S data interface");
        return -1;
    }

    /* 3. 创建 I2C 控制接口 */
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_NUM_0,
        .addr = cfg->codec_addr,
        .bus_handle = i2c_bus,
    };
    s_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    if (s_ctrl_if == NULL) {
        ESP_LOGE(TAG, "Failed to create I2C control interface");
        return -1;
    }

    /* 4. 创建 GPIO 接口 */
    s_gpio_if = audio_codec_new_gpio();
    if (s_gpio_if == NULL) {
        ESP_LOGE(TAG, "Failed to create GPIO interface");
        return -1;
    }

    /* 5. 创建 ES8388 编解码芯片驱动（匹配 xiaozhi：master 模式） */
    es8388_codec_cfg_t es8388_cfg = {
        .ctrl_if = s_ctrl_if,
        .gpio_if = s_gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .master_mode = true,
        .pa_pin = cfg->pa_pin >= 0 ? cfg->pa_pin : GPIO_NUM_NC,
        .pa_reverted = false,
        .hw_gain = {
            .pa_voltage = 5.0,
            .codec_dac_voltage = 3.3,
        },
    };
    s_codec_if = es8388_codec_new(&es8388_cfg);
    if (s_codec_if == NULL) {
        ESP_LOGE(TAG, "Failed to create ES8388 codec");
        return -1;
    }

    /* 6. 创建输出设备（DAC/播放） */
    esp_codec_dev_cfg_t out_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = s_codec_if,
        .data_if = s_data_if,
    };
    s_output_dev = esp_codec_dev_new(&out_dev_cfg);
    if (s_output_dev == NULL) {
        ESP_LOGE(TAG, "Failed to create output device");
        return -1;
    }
    esp_codec_set_disable_when_closed(s_output_dev, false);

    /* 7. 创建输入设备（ADC/录音） */
    esp_codec_dev_cfg_t in_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = s_codec_if,
        .data_if = s_data_if,
    };
    s_input_dev = esp_codec_dev_new(&in_dev_cfg);
    if (s_input_dev == NULL) {
        ESP_LOGE(TAG, "Failed to create input device");
        return -1;
    }
    esp_codec_set_disable_when_closed(s_input_dev, false);

    s_pa_pin = cfg->pa_pin;
    s_pa_expander_pin = cfg->pa_expander_pin;
    s_pa_active_low = cfg->pa_active_low;
    s_initialized = true;

    /* 设置默认音量和增益 */
    app_audio_codec_set_volume(s_volume);
    app_audio_codec_set_gain(s_gain);

    ESP_LOGI(TAG, "Audio codec ES8388 initialized (addr=0x%02X, rate=%d/%d)",
             cfg->codec_addr, cfg->input_sample_rate, cfg->output_sample_rate);

    /* 使能功放（根据 board config 选择 GPIO 或 IO 扩展芯片） */
    if (s_pa_pin >= 0) {
        gpio_set_direction(s_pa_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(s_pa_pin, s_pa_active_low ? 0 : 1);
        ESP_LOGI(TAG, "PA enabled via GPIO %d (%s)", s_pa_pin,
                 s_pa_active_low ? "active-low" : "active-high");
    } else if (s_pa_expander_pin != 0) {
        xl9555_pin_write(s_pa_expander_pin, s_pa_active_low ? 0 : 1);
        ESP_LOGI(TAG, "PA enabled via IO expander 0x%04X (%s)", s_pa_expander_pin,
                 s_pa_active_low ? "active-low" : "active-high");
    }

    return 0;
}

int app_audio_codec_open_output(int sample_rate)
{
    if (!s_initialized || s_output_dev == NULL) return -1;
    if (s_output_opened) return 0;

    /* mono（channel=1），匹配 xiaozhi 的 EnableOutput */
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
        .sample_rate = (uint32_t)sample_rate,
        .mclk_multiple = 0,
    };

    int ret = esp_codec_dev_open(s_output_dev, &fs);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Failed to open output: %d", ret);
        return -1;
    }

    /* 使能功放（仅 GPIO 模式跟随 open/close，IO 扩展模式常开避免爆音） */
    if (s_pa_pin >= 0) {
        gpio_set_direction(s_pa_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(s_pa_pin, s_pa_active_low ? 0 : 1);
    }

    /* 重新设置音量（确保 open 后生效） */
    esp_codec_dev_set_out_vol(s_output_dev, s_volume);

    /* ES8388 模拟输出音量设置（匹配 xiaozhi，使用 ctrl_if 写寄存器） */
    /* 寄存器 46-49 默认 -45dB 近乎静音，需设为 0dB（值 30） */
    uint8_t reg_val = 30;
    uint8_t regs[] = { 46, 47, 48, 49 };  /* HP_LVOL, HP_RVOL, SPK_LVOL, SPK_RVOL */
    for (int i = 0; i < 4; i++) {
        s_ctrl_if->write_reg(s_ctrl_if, regs[i], 1, &reg_val, 1);
    }

    s_output_opened = true;
    ESP_LOGI(TAG, "Audio output opened (rate=%d)", sample_rate);
    return 0;
}

int app_audio_codec_open_input(int sample_rate)
{
    if (!s_initialized || s_input_dev == NULL) return -1;
    if (s_input_opened) return 0;

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
        .sample_rate = (uint32_t)sample_rate,
        .mclk_multiple = 0,
    };

    int ret = esp_codec_dev_open(s_input_dev, &fs);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Failed to open input: %d", ret);
        return -1;
    }

    s_input_opened = true;
    ESP_LOGI(TAG, "Audio input opened (rate=%d)", sample_rate);
    return 0;
}

void app_audio_codec_close_output(void)
{
    if (!s_output_opened || s_output_dev == NULL) return;

    esp_codec_dev_close(s_output_dev);

    /* 关闭功放（仅 GPIO 模式跟随 open/close） */
    if (s_pa_pin >= 0) {
        gpio_set_level(s_pa_pin, s_pa_active_low ? 1 : 0);
    }

    s_output_opened = false;
    ESP_LOGI(TAG, "Audio output closed");
}

void app_audio_codec_close_input(void)
{
    if (!s_input_opened || s_input_dev == NULL) return;

    esp_codec_dev_close(s_input_dev);
    s_input_opened = false;
    ESP_LOGI(TAG, "Audio input closed");
}

int app_audio_codec_write(const int16_t *data, int len)
{
    if (!s_output_opened || s_output_dev == NULL) return -1;
    return esp_codec_dev_write(s_output_dev, (void *)data, len);
}

int app_audio_codec_read(int16_t *data, int len)
{
    if (!s_input_opened || s_input_dev == NULL) return -1;
    return esp_codec_dev_read(s_input_dev, (void *)data, len);
}

void app_audio_codec_set_volume(int volume)
{
    s_volume = volume;
    if (!s_initialized || s_output_dev == NULL) return;

    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    esp_codec_dev_set_out_vol(s_output_dev, volume);
}

int app_audio_codec_get_volume(void)
{
    return s_volume;
}

void app_audio_codec_set_gain(float gain_db)
{
    s_gain = gain_db;
    if (!s_initialized || s_input_dev == NULL) return;
    esp_codec_dev_set_in_gain(s_input_dev, gain_db);
}

bool app_audio_codec_is_initialized(void)
{
    return s_initialized;
}

esp_codec_dev_handle_t app_audio_codec_get_output_dev(void)
{
    return s_output_dev;
}
