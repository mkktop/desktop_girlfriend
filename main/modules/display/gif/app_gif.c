/**
 * @file app_gif.c
 * @brief GIF 播放器封装实现
 * @author mkk
 * @date 2026-05-31
 * @note 架构：
 *       gifdec 解码 GIF → ARGB8888 canvas → lv_image_dsc_t → lv_img 显示
 *       lv_timer（10ms）驱动帧切换，根据 GIF 帧延迟控制播放速率
 *       内存占用：64×64 GIF ≈ 20KB（一次 lv_malloc）
 */

#include "app_gif.h"
#include "gifdec.h"
#include "esp_log.h"
#include <string.h>

#define TAG "app_gif"

/* 定时器检查间隔（ms），越小越精确但越耗 CPU */
#define GIF_TIMER_PERIOD_MS 10

struct app_gif_player {
    lv_obj_t *img;              /* lv_img 控件 */
    gd_GIF *gif;                /* gifdec 解码上下文 */
    lv_timer_t *timer;          /* 帧动画定时器 */
    lv_image_dsc_t img_dsc;     /* 指向 gif->canvas 的图像描述符 */
    uint32_t last_tick;         /* 上一帧渲染时间戳 */
};

/* ====== 内部函数 ====== */

/**
 * @brief 帧动画定时器回调
 * @param timer LVGL 定时器
 * @note 检查帧延迟是否满足，满足则推进一帧并刷新显示
 */
static void gif_timer_cb(lv_timer_t *timer)
{
    app_gif_player_t *player = (app_gif_player_t *)lv_timer_get_user_data(timer);
    if (!player || !player->gif) return;

    /* GIF delay 单位是 1/100 秒，乘以 10 转换为 ms */
    uint32_t elapsed = lv_tick_elaps(player->last_tick);
    if (elapsed < player->gif->gce.delay * 10) return;

    /* 推进到下一帧 */
    int ret = gd_get_frame(player->gif);
    if (ret <= 0) {
        /* 动画一轮结束，从头循环 */
        gd_rewind(player->gif);
        ret = gd_get_frame(player->gif);
        if (ret <= 0) return;
    }

    /* 渲染当前帧到 canvas（ARGB8888） */
    gd_render_frame(player->gif, player->gif->canvas);

    /* 通知 LVGL 刷新图像 */
    lv_image_set_src(player->img, &player->img_dsc);
    player->last_tick = lv_tick_get();
}

/* ====== 公开接口 ====== */

app_gif_player_t *app_gif_create(lv_obj_t *parent)
{
    app_gif_player_t *player = lv_malloc(sizeof(app_gif_player_t));
    if (!player) {
        ESP_LOGE(TAG, "Failed to allocate GIF player");
        return NULL;
    }
    memset(player, 0, sizeof(*player));

    /* 创建 lv_img 控件 */
    player->img = lv_img_create(parent);
    if (!player->img) {
        lv_free(player);
        return NULL;
    }

    return player;
}

void app_gif_set_src(app_gif_player_t *player, const uint8_t *data, int data_size)
{
    if (!player || !data || data_size <= 0) return;

    /* 清理旧的 GIF 资源 */
    if (player->timer) {
        lv_timer_delete(player->timer);
        player->timer = NULL;
    }
    if (player->gif) {
        gd_close_gif(player->gif);
        player->gif = NULL;
    }

    /* 通过 gifdec 从内存数据打开 GIF */
    player->gif = gd_open_gif_data(data);
    if (!player->gif) {
        ESP_LOGW(TAG, "Failed to open GIF data (%d bytes)", data_size);
        return;
    }

    /* 构建图像描述符指向 gifdec 的 canvas（ARGB8888） */
    memset(&player->img_dsc, 0, sizeof(player->img_dsc));
    player->img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    player->img_dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    player->img_dsc.header.w = player->gif->width;
    player->img_dsc.header.h = player->gif->height;
    player->img_dsc.data = player->gif->canvas;
    player->img_dsc.data_size = player->gif->width * player->gif->height * 4;

    /* 解码并渲染第一帧 */
    if (gd_get_frame(player->gif) > 0) {
        gd_render_frame(player->gif, player->gif->canvas);
    }
    lv_image_set_src(player->img, &player->img_dsc);
    player->last_tick = lv_tick_get();

    /* 启动帧动画定时器 */
    player->timer = lv_timer_create(gif_timer_cb, GIF_TIMER_PERIOD_MS, player);
    if (!player->timer) {
        ESP_LOGW(TAG, "Failed to create GIF animation timer");
    }
}

void app_gif_destroy(app_gif_player_t *player)
{
    if (!player) return;

    /* 先停定时器，防止回调访问已释放的资源 */
    if (player->timer) {
        lv_timer_delete(player->timer);
    }
    if (player->gif) {
        gd_close_gif(player->gif);
    }
    /* lv_img 控件由页面容器的 lv_obj_clean() 自动销毁，此处不重复删除 */
    lv_free(player);
}

lv_obj_t *app_gif_get_obj(app_gif_player_t *player)
{
    return player ? player->img : NULL;
}
