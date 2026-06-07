/**
 * @file ogg_demuxer.h
 * @brief OGG 容器解复用器（纯 C 实现）
 * @author mkk
 * @date 2026-06-07
 * @note 从 xiaozhi-esp32 的 OggDemuxer C++ 类移植为纯 C，
 *       解析 OGG 页，提取 Opus 音频包。
 *       无外部依赖，固定 8KB 包缓冲区。
 */

#ifndef __OGG_DEMUXER_H__
#define __OGG_DEMUXER_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief OGG 包回调函数类型
 * @param data Opus 包数据指针
 * @param sample_rate 采样率（从 OpusHead 提取）
 * @param len 包数据长度（字节）
 * @param user_ctx 用户上下文
 */
typedef void (*ogg_packet_cb_t)(const uint8_t *data, int sample_rate,
                                size_t len, void *user_ctx);

/**
 * @brief OGG 解复用器状态
 */
typedef struct {
    /* 解析状态 */
    int8_t state;                   /* 0=FIND_PAGE, 1=PARSE_HEADER, 2=PARSE_SEGMENTS, 3=PARSE_DATA */

    /* 上下文 */
    bool packet_continued;          /* 当前包是否跨多个段 */
    uint8_t header[27];             /* OGG 页头 */
    uint8_t seg_table[255];         /* 段表 */
    uint8_t packet_buf[8192];       /* 8KB 包缓冲区 */
    size_t packet_len;              /* 缓冲区中累计的数据长度 */
    size_t seg_count;               /* 当前页段数 */
    size_t seg_index;               /* 当前处理的段索引 */
    size_t data_offset;             /* 解析当前阶段已读取的字节数 */
    size_t bytes_needed;            /* 解析当前字段还需要读取的字节数 */
    size_t seg_remaining;           /* 当前段剩余需要读取的字节数 */
    size_t body_size;               /* 数据体总大小 */
    size_t body_offset;             /* 数据体已读取的字节数 */

    /* Opus 元数据 */
    bool head_seen;                 /* 是否已找到 OpusHead */
    bool tags_seen;                 /* 是否已找到 OpusTags */
    int sample_rate;                /* 从 OpusHead 提取的采样率（默认 48000） */

    /* 回调 */
    ogg_packet_cb_t on_packet;      /* Opus 包回调 */
    void *user_ctx;                 /* 用户上下文 */
} ogg_demuxer_t;

/**
 * @brief 初始化 OGG 解复用器
 * @param ctx 解复用器上下文
 */
void ogg_demuxer_init(ogg_demuxer_t *ctx);

/**
 * @brief 重置 OGG 解复用器（可复用）
 * @param ctx 解复用器上下文
 */
void ogg_demuxer_reset(ogg_demuxer_t *ctx);

/**
 * @brief 处理 OGG 数据
 * @param ctx 解复用器上下文
 * @param data 原始 OGG 文件数据
 * @param size 数据长度
 * @return 已处理的字节数
 * @note 可增量调用，每次处理一部分数据。
 *       每提取到一个 Opus 音频包，调用 on_packet 回调。
 */
size_t ogg_demuxer_process(ogg_demuxer_t *ctx, const uint8_t *data, size_t size);

#endif /* __OGG_DEMUXER_H__ */
