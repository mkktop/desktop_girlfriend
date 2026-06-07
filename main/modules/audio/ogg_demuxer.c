/**
 * @file ogg_demuxer.c
 * @brief OGG 容器解复用器实现（纯 C）
 * @author mkk
 * @date 2026-06-07
 * @note 从 xiaozhi-esp32 的 OggDemuxer C++ 类移植，
 *       4 状态机：FIND_PAGE → PARSE_HEADER → PARSE_SEGMENTS → PARSE_DATA
 */

#include "ogg_demuxer.h"
#include "esp_log.h"
#include <string.h>

#define TAG "ogg_demuxer"

/* 解析状态常量 */
#define STATE_FIND_PAGE      0
#define STATE_PARSE_HEADER   1
#define STATE_PARSE_SEGMENTS 2
#define STATE_PARSE_DATA     3

/* 最小宏（避免依赖库） */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* ====== 公开接口 ====== */

void ogg_demuxer_init(ogg_demuxer_t *ctx)
{
    memset(ctx, 0, sizeof(ogg_demuxer_t));
    ctx->state = STATE_FIND_PAGE;
    ctx->bytes_needed = 4;  /* 寻找 "OggS" 4 字节魔数 */
    ctx->sample_rate = 48000;
}

void ogg_demuxer_reset(ogg_demuxer_t *ctx)
{
    /* 保留回调和用户上下文 */
    ogg_packet_cb_t cb = ctx->on_packet;
    void *user_ctx = ctx->user_ctx;

    memset(ctx, 0, sizeof(ogg_demuxer_t));

    ctx->state = STATE_FIND_PAGE;
    ctx->bytes_needed = 4;
    ctx->sample_rate = 48000;

    /* 恢复回调 */
    ctx->on_packet = cb;
    ctx->user_ctx = user_ctx;
}

size_t ogg_demuxer_process(ogg_demuxer_t *ctx, const uint8_t *data, size_t size)
{
    size_t processed = 0;

    while (processed < size) {
        switch (ctx->state) {

        case STATE_FIND_PAGE: {
            /* 寻找页头 "OggS" */
            if (ctx->bytes_needed < 4) {
                /* 处理不完整的 "OggS" 匹配（跨数据块） */
                size_t to_copy = MIN(size - processed, ctx->bytes_needed);
                memcpy(ctx->header + (4 - ctx->bytes_needed), data + processed, to_copy);

                processed += to_copy;
                ctx->bytes_needed -= to_copy;

                if (ctx->bytes_needed == 0) {
                    if (memcmp(ctx->header, "OggS", 4) == 0) {
                        ctx->state = STATE_PARSE_HEADER;
                        ctx->data_offset = 4;
                        ctx->bytes_needed = 27 - 4;  /* 还需要 23 字节 */
                    } else {
                        /* 匹配失败，滑动 1 字节继续 */
                        memmove(ctx->header, ctx->header + 1, 3);
                        ctx->bytes_needed = 1;
                    }
                } else {
                    return processed;  /* 数据不足 */
                }
            } else {
                /* 在数据块中查找完整的 "OggS" */
                bool found = false;
                size_t i = 0;
                size_t remaining = size - processed;

                for (; i + 4 <= remaining; i++) {
                    if (memcmp(data + processed + i, "OggS", 4) == 0) {
                        found = true;
                        break;
                    }
                }

                if (found) {
                    processed += i + 4;  /* 跳过 "OggS" */
                    ctx->state = STATE_PARSE_HEADER;
                    ctx->data_offset = 4;
                    ctx->bytes_needed = 27 - 4;
                } else {
                    /* 保存可能的部分匹配 */
                    size_t partial_len = remaining - i;
                    if (partial_len > 0) {
                        memcpy(ctx->header, data + processed + i, partial_len);
                        ctx->bytes_needed = 4 - partial_len;
                        processed += i + partial_len;
                    } else {
                        processed += i;
                    }
                    return processed;
                }
            }
            break;
        }

        case STATE_PARSE_HEADER: {
            size_t available = size - processed;

            if (available < ctx->bytes_needed) {
                memcpy(ctx->header + ctx->data_offset, data + processed, available);
                ctx->data_offset += available;
                ctx->bytes_needed -= available;
                processed += available;
                return processed;
            } else {
                memcpy(ctx->header + ctx->data_offset, data + processed, ctx->bytes_needed);
                processed += ctx->bytes_needed;
                ctx->data_offset += ctx->bytes_needed;
                ctx->bytes_needed = 0;

                /* 验证 OGG 版本 */
                if (ctx->header[4] != 0) {
                    ESP_LOGE(TAG, "Invalid OGG version: %d", ctx->header[4]);
                    ctx->state = STATE_FIND_PAGE;
                    ctx->bytes_needed = 4;
                    ctx->data_offset = 0;
                    break;
                }

                ctx->seg_count = ctx->header[26];
                if (ctx->seg_count > 0 && ctx->seg_count <= 255) {
                    ctx->state = STATE_PARSE_SEGMENTS;
                    ctx->bytes_needed = ctx->seg_count;
                    ctx->data_offset = 0;
                } else if (ctx->seg_count == 0) {
                    ctx->state = STATE_FIND_PAGE;
                    ctx->bytes_needed = 4;
                    ctx->data_offset = 0;
                } else {
                    ESP_LOGE(TAG, "Invalid seg count: %zu", ctx->seg_count);
                    ctx->state = STATE_FIND_PAGE;
                    ctx->bytes_needed = 4;
                    ctx->data_offset = 0;
                }
            }
            break;
        }

        case STATE_PARSE_SEGMENTS: {
            size_t available = size - processed;

            if (available < ctx->bytes_needed) {
                memcpy(ctx->seg_table + ctx->data_offset, data + processed, available);
                ctx->data_offset += available;
                ctx->bytes_needed -= available;
                processed += available;
                return processed;
            } else {
                memcpy(ctx->seg_table + ctx->data_offset, data + processed, ctx->bytes_needed);
                processed += ctx->bytes_needed;
                ctx->data_offset += ctx->bytes_needed;
                ctx->bytes_needed = 0;

                ctx->state = STATE_PARSE_DATA;
                ctx->seg_index = 0;
                ctx->data_offset = 0;

                /* 计算数据体总大小 */
                ctx->body_size = 0;
                for (size_t i = 0; i < ctx->seg_count; i++) {
                    ctx->body_size += ctx->seg_table[i];
                }
                ctx->body_offset = 0;
                ctx->seg_remaining = 0;
            }
            break;
        }

        case STATE_PARSE_DATA: {
            while (ctx->seg_index < ctx->seg_count && processed < size) {
                uint8_t seg_len = ctx->seg_table[ctx->seg_index];

                if (ctx->seg_remaining > 0) {
                    seg_len = (uint8_t)ctx->seg_remaining;
                } else {
                    ctx->seg_remaining = seg_len;
                }

                /* 检查缓冲区是否溢出 */
                if (ctx->packet_len + seg_len > sizeof(ctx->packet_buf)) {
                    ESP_LOGE(TAG, "Packet buffer overflow: %zu + %u > %zu",
                             ctx->packet_len, seg_len, sizeof(ctx->packet_buf));
                    ctx->state = STATE_FIND_PAGE;
                    ctx->packet_len = 0;
                    ctx->packet_continued = false;
                    ctx->seg_remaining = 0;
                    ctx->bytes_needed = 4;
                    return processed;
                }

                /* 复制段数据 */
                size_t to_copy = MIN(size - processed, (size_t)seg_len);
                memcpy(ctx->packet_buf + ctx->packet_len, data + processed, to_copy);

                processed += to_copy;
                ctx->packet_len += to_copy;
                ctx->body_offset += to_copy;
                ctx->seg_remaining -= to_copy;

                if (ctx->seg_remaining > 0) {
                    return processed;  /* 段不完整 */
                }

                /* 段完整，检查是否为续接段 */
                bool seg_continued = (ctx->seg_table[ctx->seg_index] == 255);

                if (!seg_continued) {
                    /* 包结束 */
                    if (ctx->packet_len > 0) {
                        if (!ctx->head_seen) {
                            if (ctx->packet_len >= 8 &&
                                memcmp(ctx->packet_buf, "OpusHead", 8) == 0) {
                                ctx->head_seen = true;
                                if (ctx->packet_len >= 19) {
                                    ctx->sample_rate = ctx->packet_buf[12] |
                                                       (ctx->packet_buf[13] << 8) |
                                                       (ctx->packet_buf[14] << 16) |
                                                       (ctx->packet_buf[15] << 24);
                                    ESP_LOGD(TAG, "OpusHead: rate=%d", ctx->sample_rate);
                                }
                                ctx->packet_len = 0;
                                ctx->packet_continued = false;
                                ctx->seg_index++;
                                ctx->seg_remaining = 0;
                                continue;
                            }
                        }
                        if (!ctx->tags_seen) {
                            if (ctx->packet_len >= 8 &&
                                memcmp(ctx->packet_buf, "OpusTags", 8) == 0) {
                                ctx->tags_seen = true;
                                ESP_LOGD(TAG, "OpusTags found");
                                ctx->packet_len = 0;
                                ctx->packet_continued = false;
                                ctx->seg_index++;
                                ctx->seg_remaining = 0;
                                continue;
                            }
                        }
                        if (ctx->head_seen && ctx->tags_seen) {
                            if (ctx->on_packet) {
                                ctx->on_packet(ctx->packet_buf, ctx->sample_rate,
                                               ctx->packet_len, ctx->user_ctx);
                            }
                        } else {
                            ESP_LOGW(TAG, "Packet before OpusHead/Tags, discarded");
                        }
                    }
                    ctx->packet_len = 0;
                    ctx->packet_continued = false;
                } else {
                    ctx->packet_continued = true;
                }

                ctx->seg_index++;
                ctx->seg_remaining = 0;
            }

            if (ctx->seg_index == ctx->seg_count) {
                if (ctx->body_offset < ctx->body_size) {
                    ESP_LOGW(TAG, "Incomplete body: %zu/%zu",
                             ctx->body_offset, ctx->body_size);
                }

                if (!ctx->packet_continued) {
                    ctx->packet_len = 0;
                }

                /* 下一页 */
                ctx->state = STATE_FIND_PAGE;
                ctx->bytes_needed = 4;
                ctx->data_offset = 0;
            }
            break;
        }

        default:
            ESP_LOGE(TAG, "Invalid state: %d", ctx->state);
            ogg_demuxer_reset(ctx);
            return processed;
        }
    }

    return processed;
}
