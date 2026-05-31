/**
 * @file app_font.h
 * @brief 字体管理器头文件
 * @author mkk
 * @date 2026-05-31
 * @note 实现两层字体策略：
 *       内置基础字体（编译时） + CBin通用字体（运行时从资源分区加载），
 *       对外提供统一的字体获取接口，UI代码无需关心字体来源
 */

#ifndef __APP_FONT_H__
#define __APP_FONT_H__

#include "lvgl.h"
#include "esp_mmap_assets.h"

/**
 * @brief 初始化字体管理器
 * @note 设置内置字体为默认，尝试从资源分区加载 CBin 字体，
 *       如加载成功则替换为 CBin 字体。
 *       必须在 app_display_init() 之后、ui_manager_init() 之前调用
 */
void app_font_init(void);

/**
 * @brief 获取当前文本字体（CJK）
 * @return const lv_font_t* 文本字体指针，始终返回有效字体
 * @note 优先返回 CBin 运行时字体，否则返回内置基础字体
 */
const lv_font_t *app_font_get_text(void);

/**
 * @brief 获取图标字体（FontAwesome 16px）
 * @return const lv_font_t* 图标字体指针
 */
const lv_font_t *app_font_get_icon(void);

/**
 * @brief 获取资源分区句柄
 * @return mmap_assets_handle_t 资源分区句柄，NULL 表示不可用
 * @note 供其他模块（如表情系统）访问 assets 分区中的文件
 */
mmap_assets_handle_t app_font_get_assets_handle(void);

#endif /* __APP_FONT_H__ */
