/**
 * @file app_audio.h
 * @brief 音频服务层公共接口
 * @author mkk
 * @date 2026-06-06
 * @note 提供录音/播放的高级 API，内部管理 FreeRTOS 任务和缓冲区。
 *       通过 board config 判断是否有音频硬件，无音频时接口为空操作。
 */

#ifndef __APP_AUDIO_H__
#define __APP_AUDIO_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief 初始化音频子系统
 * @note 读取板卡配置，若有音频硬件则初始化编解码芯片，
 *       若无音频硬件则跳过（后续 API 调用为空操作）
 */
void app_audio_init(void);

/**
 * @brief 播放提示音（正弦波）
 * @param freq_hz 频率（Hz，如 1000）
 * @param duration_ms 持续时间（毫秒）
 * @return 0 成功，-1 失败
 */
int app_audio_play_tone(int freq_hz, int duration_ms);

/**
 * @brief 播放 OGG/Opus 提示音
 * @param ogg_data 内嵌 OGG 文件数据指针
 * @param ogg_len 数据长度（字节）
 * @return 0 成功，-1 失败
 * @note 数据来自 EMBED_FILES 编译嵌入的 OGG 文件，
 *       通过 OGG 解复用器提取 Opus 包，解码为 PCM 后播放
 */
int app_audio_play_sound(const uint8_t *ogg_data, size_t ogg_len);

/**
 * @brief 设置输出音量
 * @param volume 音量（0-100）
 */
void app_audio_set_volume(int volume);

/**
 * @brief 获取输出音量
 * @return 当前音量（0-100）
 */
int app_audio_get_volume(void);

/**
 * @brief 检查音频子系统是否可用
 * @return true 音频硬件已初始化
 */
bool app_audio_is_available(void);

#endif /* __APP_AUDIO_H__ */
