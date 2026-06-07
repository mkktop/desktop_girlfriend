/**
 * @file app_audio_codec.h
 * @brief 音频编解码芯片抽象层
 * @author mkk
 * @date 2026-06-06
 * @note 封装 esp_codec_dev 组件，提供统一的音频编解码芯片初始化和读写接口。
 *       通过 board config 中的 audio_i2s_cfg_t 适配不同芯片（ES8388、ES8311 等）。
 */

#ifndef __APP_AUDIO_CODEC_H__
#define __APP_AUDIO_CODEC_H__

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"

/**
 * @brief 初始化音频编解码芯片
 * @param i2c_bus 已创建的 I2C 总线句柄（可为 NULL，内部自动创建）
 * @return 0 成功，-1 失败
 * @note 从 board_get_instance() 读取音频配置，
 *       若 i2s_port == -1 则跳过初始化（无音频硬件）
 */
int app_audio_codec_init(i2c_master_bus_handle_t i2c_bus);

/**
 * @brief 打开音频输出（DAC/播放）
 * @param sample_rate 采样率（如 16000）
 * @return 0 成功，-1 失败
 */
int app_audio_codec_open_output(int sample_rate);

/**
 * @brief 打开音频输入（ADC/录音）
 * @param sample_rate 采样率（如 16000）
 * @return 0 成功，-1 失败
 */
int app_audio_codec_open_input(int sample_rate);

/**
 * @brief 关闭音频输出
 */
void app_audio_codec_close_output(void);

/**
 * @brief 关闭音频输入
 */
void app_audio_codec_close_input(void);

/**
 * @brief 写入 PCM 数据到音频输出（播放）
 * @param data PCM 采样数据（int16_t 数组）
 * @param len 数据长度（字节）
 * @return 实际写入字节数，-1 失败
 */
int app_audio_codec_write(const int16_t *data, int len);

/**
 * @brief 从音频输入读取 PCM 数据（录音）
 * @param data 输出缓冲区
 * @param len 缓冲区长度（字节）
 * @return 实际读取字节数，-1 失败
 */
int app_audio_codec_read(int16_t *data, int len);

/**
 * @brief 设置输出音量
 * @param volume 音量（0-100）
 */
void app_audio_codec_set_volume(int volume);

/**
 * @brief 获取输出音量
 * @return 当前音量（0-100）
 */
int app_audio_codec_get_volume(void);

/**
 * @brief 设置输入增益
 * @param gain_db 增益（dB，如 24.0）
 */
void app_audio_codec_set_gain(float gain_db);

/**
 * @brief 检查音频编解码芯片是否已初始化
 * @return true 已初始化
 */
bool app_audio_codec_is_initialized(void);

/**
 * @brief 获取输出设备句柄（用于直接寄存器操作）
 * @return 输出设备句柄，未初始化返回 NULL
 */
esp_codec_dev_handle_t app_audio_codec_get_output_dev(void);

#endif /* __APP_AUDIO_CODEC_H__ */
