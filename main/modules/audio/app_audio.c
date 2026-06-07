/**
 * @file app_audio.c
 * @brief 音频服务层实现
 * @author mkk
 * @date 2026-06-06
 * @note 提供播放提示音等高级 API，内部调用 app_audio_codec 完成底层操作。
 *       OGG/Opus 解码在独立 FreeRTOS 任务中执行，避免阻塞 main task。
 *       无音频硬件时所有 API 为空操作，不影响其他板卡。
 */

#include "app_audio.h"
#include "app_audio_codec.h"
#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "ogg_demuxer.h"
#include "esp_opus_dec.h"
#include "esp_audio_types.h"
#include "app_event.h"
#include <math.h>
#include <string.h>

#define TAG "app_audio"

/* ====== 音频播放任务配置 ====== */
#define AUDIO_PLAYBACK_TASK_STACK   (24 * 1024)  /* 任务栈大小（24KB，预留编码+重采样空间） */
#define AUDIO_PLAYBACK_TASK_PRIO    2       /* 任务优先级（匹配 xiaozhi OpusCodecTask） */
#define AUDIO_PLAYBACK_QUEUE_SIZE   4       /* 播放队列深度 */
#define AUDIO_PLAYBACK_CORE         0       /* 绑定核心（Core 0，LVGL 在 Core 1） */

/* Opus 解码 PCM 缓冲区：48kHz × 60ms × 2 字节（mono）= 5760，取整 8KB */
#define OPUS_PCM_BUF_SIZE 8192

/* ====== 播放任务消息 ====== */

typedef struct {
    const uint8_t *ogg_data;      /* OGG 文件数据指针（EMBED_FILES 符号） */
    size_t ogg_len;               /* 数据长度（字节） */
    TaskHandle_t caller;          /* 调用者任务句柄（用于通知完成） */
} audio_play_msg_t;

/* ====== 模块状态 ====== */

static TaskHandle_t s_playback_task = NULL;
static QueueHandle_t s_play_queue = NULL;

/* 嵌入提示音文件 */
extern const char _binary_success_ogg_start[] asm("_binary_success_ogg_start");
extern const char _binary_success_ogg_end[] asm("_binary_success_ogg_end");
extern const char _binary_wifi_config_ogg_start[] asm("_binary_wifi_config_ogg_start");
extern const char _binary_wifi_config_ogg_end[] asm("_binary_wifi_config_ogg_end");
extern const char _binary_wifi_connected_ogg_start[] asm("_binary_wifi_connected_ogg_start");
extern const char _binary_wifi_connected_ogg_end[] asm("_binary_wifi_connected_ogg_end");

/* ====== OGG 解码播放 ====== */

/* OGG 播放上下文（传递给 demuxer 回调） */
typedef struct {
    void *opus_dec;              /* Opus 解码器句柄（延迟创建） */
    int16_t *pcm_buf;            /* PCM 输出缓冲区 */
    bool decoder_opened;         /* 解码器是否已打开 */
    bool output_opened;          /* 输出是否已打开 */
    bool decode_ok;              /* 解码是否成功 */
    int packet_count;            /* 已解码的包数 */
} ogg_play_ctx_t;

/**
 * @brief OGG demuxer 回调：解码 Opus 包并写入音频输出
 * @note 每个 Opus 音频包调用一次，首次回调时根据 OGG 实际采样率创建解码器和打开输出
 */
static void ogg_packet_callback(const uint8_t *data, int sample_rate,
                                size_t len, void *user_ctx)
{
    ogg_play_ctx_t *ctx = (ogg_play_ctx_t *)user_ctx;

    if (len == 0) return;

    /* 首次回调时：用 OGG 文件的实际采样率打开解码器和音频输出 */
    if (!ctx->decoder_opened) {
        esp_opus_dec_cfg_t opus_cfg = {
            .sample_rate = (uint32_t)sample_rate,
            .channel = ESP_AUDIO_MONO,
            .frame_duration = ESP_OPUS_DEC_FRAME_DURATION_60_MS,
            .self_delimited = false,
        };
        esp_audio_err_t ret = esp_opus_dec_open(&opus_cfg, sizeof(opus_cfg), &ctx->opus_dec);
        if (ret != ESP_AUDIO_ERR_OK || ctx->opus_dec == NULL) {
            ESP_LOGE(TAG, "Failed to open Opus decoder (%dHz): %d", sample_rate, ret);
            ctx->decode_ok = false;
            return;
        }
        ctx->decoder_opened = true;
        ESP_LOGI(TAG, "Opus decoder opened at %dHz", sample_rate);

        if (app_audio_codec_open_output(sample_rate) != 0) {
            ESP_LOGE(TAG, "Failed to open audio output for OGG playback");
            ctx->decode_ok = false;
            return;
        }
        ctx->output_opened = true;
    }

    /* 设置解码输入 */
    esp_audio_dec_in_raw_t raw = {
        .buffer = (uint8_t *)data,
        .len = (uint32_t)len,
        .consumed = 0,
        .frame_recover = ESP_AUDIO_DEC_RECOVERY_NONE,
    };

    esp_audio_dec_out_frame_t frame = {
        .buffer = (uint8_t *)ctx->pcm_buf,
        .len = OPUS_PCM_BUF_SIZE,
        .needed_size = 0,
        .decoded_size = 0,
    };

    esp_audio_dec_info_t dec_info = {0};

    /* 解码 Opus 包 → PCM */
    esp_audio_err_t ret = esp_opus_dec_decode(ctx->opus_dec, &raw, &frame, &dec_info);
    if (ret == ESP_AUDIO_ERR_OK && frame.decoded_size > 0) {
        /* 写入音频输出 */
        app_audio_codec_write(ctx->pcm_buf, (int)frame.decoded_size);
        ctx->packet_count++;
    } else if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGW(TAG, "Opus decode error: %d (len=%zu)", ret, len);
    }
}

/**
 * @brief 播放单个 OGG/Opus 音频（内部函数，在播放任务中执行）
 * @return 0 成功，-1 失败
 * @note 解码器和音频输出延迟到首次回调时创建，自动匹配 OGG 文件的实际采样率
 */
static int playback_ogg_sound(const uint8_t *ogg_data, size_t ogg_len)
{
    /* 1. 分配 PCM 输出缓冲区 */
    int16_t *pcm_buf = heap_caps_malloc(OPUS_PCM_BUF_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (pcm_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate PCM buffer");
        return -1;
    }

    /* 2. 准备播放上下文和 OGG demuxer */
    /* ogg_demuxer_t 内含 8KB packet_buf，必须堆分配避免栈溢出 */
    ogg_play_ctx_t play_ctx = {
        .opus_dec = NULL,
        .pcm_buf = pcm_buf,
        .decoder_opened = false,
        .output_opened = false,
        .decode_ok = true,
        .packet_count = 0,
    };

    ogg_demuxer_t *demuxer = heap_caps_malloc(sizeof(ogg_demuxer_t), MALLOC_CAP_SPIRAM);
    if (demuxer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate OGG demuxer (%zu bytes)", sizeof(ogg_demuxer_t));
        free(pcm_buf);
        return -1;
    }

    ogg_demuxer_init(demuxer);
    demuxer->on_packet = ogg_packet_callback;
    demuxer->user_ctx = &play_ctx;

    /* 4. 处理全部 OGG 数据 */
    size_t processed = ogg_demuxer_process(demuxer, ogg_data, ogg_len);
    if (processed < ogg_len) {
        ESP_LOGW(TAG, "OGG demuxer processed %zu/%zu bytes", processed, ogg_len);
    }

    /* 5. 等待最后的 DMA 数据播放完成 */
    if (play_ctx.packet_count > 0) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    /* 6. 关闭输出和解码器 */
    if (play_ctx.output_opened) {
        app_audio_codec_close_output();
    }
    if (play_ctx.decoder_opened) {
        esp_opus_dec_close(play_ctx.opus_dec);
    }

    free(pcm_buf);
    free(demuxer);

    ESP_LOGI(TAG, "OGG sound played: %d packets, %zu/%zu bytes",
             play_ctx.packet_count, processed, ogg_len);

    return play_ctx.decode_ok ? 0 : -1;
}

/* ====== 播放任务 ====== */

/**
 * @brief 音频播放任务
 * @note 从队列接收播放请求，在独立任务上下文中完成 OGG 解码和音频输出。
 *       通过 task notification 通知调用者播放完成。
 */
static void audio_playback_task(void *arg)
{
    audio_play_msg_t msg;

    ESP_LOGI(TAG, "Audio playback task started (core=%d, stack=%d, prio=%d)",
             AUDIO_PLAYBACK_CORE, AUDIO_PLAYBACK_TASK_STACK, AUDIO_PLAYBACK_TASK_PRIO);

    while (1) {
        /* 等待播放请求 */
        if (xQueueReceive(s_play_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /* 执行 OGG 解码播放 */
        playback_ogg_sound(msg.ogg_data, msg.ogg_len);

        /* 通知调用者播放完成 */
        if (msg.caller != NULL) {
            xTaskNotifyGive(msg.caller);
        }
    }
}

/* ====== 公开接口 ====== */

/**
 * @brief 播放内嵌 OGG/Opus 提示音（阻塞等待播放完成）
 * @param ogg_data 内嵌 OGG 文件数据指针（EMBED_FILES 生成）
 * @param ogg_len 数据长度（字节）
 * @return 0 成功，-1 失败
 * @note 将播放请求发送到独立任务执行，当前任务阻塞等待完成。
 *       通过 task notification 实现同步，不占用调用者栈空间。
 */
int app_audio_play_sound(const uint8_t *ogg_data, size_t ogg_len)
{
    if (!app_audio_is_available()) return -1;
    if (ogg_data == NULL || ogg_len == 0) {
        ESP_LOGE(TAG, "Invalid OGG data");
        return -1;
    }
    if (s_play_queue == NULL) {
        ESP_LOGE(TAG, "Audio playback task not ready");
        return -1;
    }

    audio_play_msg_t msg = {
        .ogg_data = ogg_data,
        .ogg_len = ogg_len,
        .caller = xTaskGetCurrentTaskHandle(),
    };

    /* 清除可能残留的通知 */
    ulTaskNotifyValueClear(NULL, ULONG_MAX);

    /* 发送到播放队列 */
    if (xQueueSend(s_play_queue, &msg, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Audio play queue full");
        return -1;
    }

    /* 阻塞等待播放任务完成通知 */
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000));

    return 0;
}

/**
 * @brief 生成并播放一段正弦波提示音
 * @param freq_hz 频率（Hz，如 1000）
 * @param duration_ms 持续时间（毫秒）
 * @return 0 成功，-1 失败
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
 * @brief 事件观察者：响应 WiFi 事件播放对应提示音
 * @param event_bits 触发的事件位
 * @param user_ctx 未使用
 * @note 在主事件循环中执行，此时音频子系统已初始化完毕
 */
static void audio_event_handler(EventBits_t event_bits, void *user_ctx)
{
    if (event_bits & APP_EVENT_WIFI_CONFIG_ENTER) {
        size_t ogg_size = _binary_wifi_config_ogg_end - _binary_wifi_config_ogg_start;
        app_audio_play_sound((const uint8_t *)_binary_wifi_config_ogg_start, ogg_size);
    }

    if (event_bits & APP_EVENT_WIFI_GOT_IP) {
        size_t ogg_size = _binary_wifi_connected_ogg_end - _binary_wifi_connected_ogg_start;
        app_audio_play_sound((const uint8_t *)_binary_wifi_connected_ogg_start, ogg_size);
    }
}

/**
 * @brief 初始化音频子系统
 * @note 创建编解码芯片驱动、播放任务和消息队列。
 *       注册事件观察者，通过主事件循环响应 WiFi 提示音播放。
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

    /* 获取 I2C 控制总线（板卡自行决定来源：共享 XL9555 总线 / 独立创建） */
    i2c_master_bus_handle_t i2c_bus = NULL;
    if (board->get_audio_i2c_bus) {
        i2c_bus = (i2c_master_bus_handle_t)board->get_audio_i2c_bus();
    }

    /* 初始化编解码芯片（内部包含 PA 使能） */
    if (app_audio_codec_init(i2c_bus) != 0) {
        ESP_LOGE(TAG, "Failed to initialize audio codec");
        return;
    }

    /* 创建播放消息队列 */
    s_play_queue = xQueueCreate(AUDIO_PLAYBACK_QUEUE_SIZE, sizeof(audio_play_msg_t));
    if (s_play_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create audio play queue");
        return;
    }

    /* 创建音频播放任务（绑定 Core 0，与 LVGL 的 Core 1 分离） */
    BaseType_t ret = xTaskCreatePinnedToCore(
        audio_playback_task,
        "audio_play",
        AUDIO_PLAYBACK_TASK_STACK,
        NULL,
        AUDIO_PLAYBACK_TASK_PRIO,
        &s_playback_task,
        AUDIO_PLAYBACK_CORE
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio playback task");
        vQueueDelete(s_play_queue);
        s_play_queue = NULL;
        return;
    }

    /* 注册事件观察者：配网引导音 + WiFi 连接成功音 */
    app_event_register_handler(audio_event_handler,
        APP_EVENT_WIFI_CONFIG_ENTER | APP_EVENT_WIFI_GOT_IP, NULL);

    /* 播放启动提示音（OGG/Opus） */
    size_t ogg_size = _binary_success_ogg_end - _binary_success_ogg_start;
    app_audio_play_sound((const uint8_t *)_binary_success_ogg_start, ogg_size);

    ESP_LOGI(TAG, "Audio subsystem initialized");
}
