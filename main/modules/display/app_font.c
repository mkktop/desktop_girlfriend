/**
 * @file app_font.c
 * @brief 字体管理器实现
 * @author mkk
 * @date 2026-05-31
 * @note 两层字体策略：
 *       Layer 1 - 内置 font_puhui_basic_16_4（编译时链接，ASCII + 基础CJK）
 *       Layer 2 - CBin font_puhui_common_16_4.bin（运行时从 assets 分区 mmap 加载）
 *       使用 esp_mmap_assets 官方组件管理资源分区，
 *       使用 xiaozhi-fonts 组件提供的 CBin 加载器
 */

#include "app_font.h"
#include "board.h"
#include "esp_log.h"
#include "esp_mmap_assets.h"
#include "mmap_generate_assets.h"
#include "cbin_font.h"

/* 内置字体声明（来自 xiaozhi-fonts 组件，编译时链接） */
LV_FONT_DECLARE(font_puhui_basic_16_4)
LV_FONT_DECLARE(font_awesome_16_4)

#define TAG "app_font"

/* 资源分区句柄 */
static mmap_assets_handle_t s_assets_handle = NULL;

/* 当前激活的文本字体指针 */
static lv_font_t *s_text_font = NULL;

/* CBin 运行时字体（从资源分区加载，需要时释放） */
static lv_font_t *s_cbin_font = NULL;

/* 图标字体（内置，不需要运行时加载） */
static lv_font_t *s_icon_font = NULL;

/**
 * @brief 初始化字体管理器
 */
void app_font_init(void)
{
    /* 1. 设置内置基础字体为默认（保证始终有可用字体） */
    s_text_font = &font_puhui_basic_16_4;
    s_icon_font = &font_awesome_16_4;
    ESP_LOGI(TAG, "Built-in font: font_puhui_basic_16_4");

    /* 2. 尝试从资源分区加载 CBin 通用字体 */
    const mmap_assets_config_t config = {
        .partition_label = "assets",
        .max_files = MMAP_ASSETS_FILES,
        .checksum = MMAP_ASSETS_CHECKSUM,
        .flags = {
            .mmap_enable = true,
            .use_fs = false,
            .app_bin_check = true,
        },
    };

    esp_err_t ret = mmap_assets_new(&config, &s_assets_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Assets partition not available (0x%x), using built-in font", ret);
        return;
    }

    /* 3. 从资源分区获取 CBin 字体数据 */
    const uint8_t *font_data = mmap_assets_get_mem(s_assets_handle, MMAP_ASSETS_FONT_PUHUI_COMMON_16_4_BIN);
    if (!font_data) {
        ESP_LOGW(TAG, "Failed to get font data from assets");
        return;
    }

    /* 4. 创建 CBin 运行时字体 */
    s_cbin_font = cbin_font_create((uint8_t *)font_data);
    if (s_cbin_font) {
        s_text_font = s_cbin_font;
        ESP_LOGI(TAG, "CBin font loaded from assets partition");
    } else {
        ESP_LOGW(TAG, "CBin font creation failed, using built-in font");
    }
}

/**
 * @brief 获取当前文本字体
 */
lv_font_t *app_font_get_text(void)
{
    return s_text_font;
}

/**
 * @brief 获取图标字体
 */
lv_font_t *app_font_get_icon(void)
{
    return s_icon_font;
}
