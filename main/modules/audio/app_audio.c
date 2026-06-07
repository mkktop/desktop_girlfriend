/**
 * @file app_audio.c
 * @brief 音频服务层实现
 * @author mkk
 * @date 2026-06-06
 * @note 提供播放提示音等高级 API，内部调用 app_audio_codec 完成底层操作。
 *       无音频硬件时所有 API 为空操作，不影响其他板卡。
 */

#include "app_audio.h"
#include "app_audio_codec.h"
#include "board.h"
#include "xl9555.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

#define TAG "app_audio"

/**
 * @brief 生成并播放一段正弦波提示音
 * @note 使用 mono 数据（匹配 channel=1 配置，esp_codec_dev 自动扩展为 stereo I2S 帧）
 *       采样率从 board config 动态读取，适配不同板卡
 */
int app_audio_play_tone(int freq_hz, int duration_ms)
{
    if (!app_audio_is_available()) return -1;

    const board_t *board = board_get_instance();
    int sample_rate = board->audio.output_sample_rate;
    int total_samples = sample_rate * duration_ms / 1000;

    /* Mono：每个采样一个 int16_t */
    int buf_size = total_samples * sizeof(int16_t);
    int16_t *buf = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate tone buffer (%d bytes)", buf_size);
        return -1;
    }

    /* 生成正弦波 */
    float amplitude = 0.3f * 32767.0f;  /* 30% 音量避免削波 */
    int fade_samples = sample_rate * 10 / 1000;
    for (int i = 0; i < total_samples; i++) {
        float t = (float)i / sample_rate;
        int16_t sample = (int16_t)(amplitude * sinf(2.0f * (float)M_PI * freq_hz * t));
        /* 淡入淡出（前后 10ms） */
        if (i < fade_samples) {
            sample = (int16_t)((float)sample * (float)i / (float)fade_samples);
        } else if (i > total_samples - fade_samples) {
            sample = (int16_t)((float)sample * (float)(total_samples - i) / (float)fade_samples);
        }
        buf[i] = sample;  /* 单声道 */
    }

    /* 打开输出并播放 */
    app_audio_codec_open_output(sample_rate);

    int written = app_audio_codec_write(buf, buf_size);

    /* 等待 DMA 播放完成 */
    vTaskDelay(pdMS_TO_TICKS(duration_ms + 200));

    app_audio_codec_close_output();

    free(buf);

    if (written < 0) {
        ESP_LOGE(TAG, "Failed to play tone");
        return -1;
    }

    ESP_LOGI(TAG, "Tone played: %dHz, %dms", freq_hz, duration_ms);
    return 0;
}

void app_audio_set_volume(int volume)
{
    if (!app_audio_is_available()) return;
    app_audio_codec_set_volume(volume);
}

int app_audio_get_volume(void)
{
    return app_audio_codec_get_volume();
}

bool app_audio_is_available(void)
{
    return app_audio_codec_is_initialized();
}

/**
 * @brief 初始化音频子系统
 * @note PA 使能已移入 app_audio_codec_init() 内部，根据 board config 自动选择
 *       GPIO 或 IO 扩展芯片方式
 */
void app_audio_init(void)
{
    const board_t *board = board_get_instance();
    const audio_i2s_cfg_t *cfg = &board->audio;

    /* 无音频硬件的板卡直接跳过 */
    if (cfg->i2s_port < 0) {
        ESP_LOGI(TAG, "No audio hardware, skipping audio init");
        return;
    }

    /* 获取共享 I2C 总线（XL9555 已初始化时复用） */
    i2c_master_bus_handle_t i2c_bus = xl9555_get_i2c_bus();

    /* 初始化编解码芯片（内部包含 PA 使能） */
    if (app_audio_codec_init(i2c_bus) != 0) {
        ESP_LOGE(TAG, "Failed to initialize audio codec");
        return;
    }

    /* 播放启动提示音 */
    app_audio_codec_diagnostic();

    ESP_LOGI(TAG, "Audio subsystem initialized");
}
