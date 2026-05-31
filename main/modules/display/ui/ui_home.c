/**
 * @file ui_home.c
 * @brief 首页UI实现（动态 GIF 表情界面）
 * @author mkk
 * @date 2026-05-31
 * @note 首页布局：
 *       居中显示 64x64 Noto Emoji 动态 GIF（来自 assets 分区），
 *       底部显示对话字幕文字，
 *       支持 21 种表情切换，使用 gifdec 解码器 + lv_img 播放动画
 */

#include "ui_home.h"
#include "app_font.h"
#include "app_gif.h"
#include "app_event.h"
#include "mmap_generate_assets.h"
#include "lvgl.h"
#include <string.h>

/* ====== 表情映射表 ====== */

typedef struct {
    const char *name;               /* 表情名称 */
    int asset_id;                   /* assets 分区中的文件索引（enum MMAP_ASSETS_LISTS） */
} emotion_map_t;

static const emotion_map_t s_emotion_map[] = {
    {"neutral",      MMAP_ASSETS_NEUTRAL_GIF},
    {"happy",        MMAP_ASSETS_HAPPY_GIF},
    {"laughing",     MMAP_ASSETS_LAUGHING_GIF},
    {"funny",        MMAP_ASSETS_FUNNY_GIF},
    {"sad",          MMAP_ASSETS_SAD_GIF},
    {"angry",        MMAP_ASSETS_ANGRY_GIF},
    {"crying",       MMAP_ASSETS_CRYING_GIF},
    {"loving",       MMAP_ASSETS_LOVING_GIF},
    {"embarrassed",  MMAP_ASSETS_EMBARRASSED_GIF},
    {"surprised",    MMAP_ASSETS_SURPRISED_GIF},
    {"shocked",      MMAP_ASSETS_SHOCKED_GIF},
    {"thinking",     MMAP_ASSETS_THINKING_GIF},
    {"winking",      MMAP_ASSETS_WINKING_GIF},
    {"cool",         MMAP_ASSETS_COOL_GIF},
    {"relaxed",      MMAP_ASSETS_RELAXED_GIF},
    {"delicious",    MMAP_ASSETS_DELICIOUS_GIF},
    {"kissy",        MMAP_ASSETS_KISSY_GIF},
    {"confident",    MMAP_ASSETS_CONFIDENT_GIF},
    {"sleepy",       MMAP_ASSETS_SLEEPY_GIF},
    {"silly",        MMAP_ASSETS_SILLY_GIF},
    {"confused",     MMAP_ASSETS_CONFUSED_GIF},
};

#define EMOTION_COUNT (sizeof(s_emotion_map) / sizeof(s_emotion_map[0]))

/* ====== 页面对象 ====== */

static app_gif_player_t *s_gif_player = NULL;   /* GIF 播放器 */
static lv_obj_t *s_subtitle_label = NULL;        /* 底部对话字幕 */

/* ====== 内部函数 ====== */

/**
 * @brief 从 assets 分区加载 GIF 并设置到播放器
 * @param asset_id assets 分区文件索引
 */
static void set_gif_from_assets(int asset_id)
{
    mmap_assets_handle_t handle = app_font_get_assets_handle();
    if (!handle) return;

    const uint8_t *data = mmap_assets_get_mem(handle, asset_id);
    int data_size = mmap_assets_get_size(handle, asset_id);
    if (!data || data_size <= 0) return;

    app_gif_set_src(s_gif_player, data, data_size);
}

/* ====== 页面接口实现 ====== */

/**
 * @brief 创建首页UI
 * @param parent 页面容器
 */
void ui_home_create(lv_obj_t *parent)
{
    /* 创建 GIF 播放器（内含 lv_img 控件） */
    s_gif_player = app_gif_create(parent);
    set_gif_from_assets(MMAP_ASSETS_NEUTRAL_GIF);
    lv_obj_t *gif_obj = app_gif_get_obj(s_gif_player);
    lv_obj_align(gif_obj, LV_ALIGN_CENTER, 0, -20);

    /* 底部对话字幕栏 */
    s_subtitle_label = lv_label_create(parent);
    lv_label_set_text(s_subtitle_label, "");
    lv_obj_set_style_text_font(s_subtitle_label, app_font_get_text(), 0);
    lv_label_set_long_mode(s_subtitle_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_subtitle_label, 220);
    lv_obj_set_style_text_align(s_subtitle_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_subtitle_label, LV_ALIGN_BOTTOM_MID, 0, -20);
}

/**
 * @brief 销毁首页资源
 * @note 在页面切换前调用，释放 GIF 播放器资源
 */
void ui_home_destroy(void)
{
    if (s_gif_player) {
        app_gif_destroy(s_gif_player);
        s_gif_player = NULL;
    }
    s_subtitle_label = NULL;
}

/**
 * @brief 首页事件处理
 * @param event_bits 当前触发的事件位
 */
void ui_home_on_event(EventBits_t event_bits)
{
    /* 首页特有的事件处理（预留） */
    (void)event_bits;
}

/**
 * @brief 设置表情
 * @param emotion 表情名称
 */
void ui_home_set_emotion(const char *emotion)
{
    if (!s_gif_player || !emotion) return;

    /* 在映射表中查找对应 asset */
    for (int i = 0; i < (int)EMOTION_COUNT; i++) {
        if (strcmp(emotion, s_emotion_map[i].name) == 0) {
            set_gif_from_assets(s_emotion_map[i].asset_id);
            return;
        }
    }

    /* 未找到则使用 neutral */
    set_gif_from_assets(MMAP_ASSETS_NEUTRAL_GIF);
}

/**
 * @brief 设置底部对话字幕
 * @param text 字幕文字
 */
void ui_home_set_subtitle(const char *text)
{
    if (!s_subtitle_label) return;
    lv_label_set_text(s_subtitle_label, text ? text : "");
}
