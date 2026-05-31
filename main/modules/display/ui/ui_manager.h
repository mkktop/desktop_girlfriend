/**
 * @file ui_manager.h
 * @brief UI页面管理器头文件（三层容器架构）
 * @author mkk
 * @date 2026-05-31
 * @note 三层容器架构：
 *       sys_container（系统层）→ status_bar + page_container
 *       系统层持久存在（状态栏、时钟），页面层可切换，
 *       未来可扩展 overlay_container（聊天浮层）
 */

#ifndef __UI_MANAGER_H__
#define __UI_MANAGER_H__

#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/* ============================================================
 * 页面 ID 枚举
 * 添加新页面时在此增加，并在 ui_manager.c 的 s_pages 注册
 * ============================================================ */
typedef enum {
    UI_PAGE_HOME = 0,
    UI_PAGE_WIFI_CONFIG,
    UI_PAGE_COUNT  /* 总数，用于数组边界 */
} ui_page_id_t;

/* ============================================================
 * 页面接口
 * 每个页面模块实现 create()，可选实现 on_event()
 * ============================================================ */

/**
 * @brief 页面创建函数类型
 * @param parent 页面容器，所有组件应创建在 parent 下
 */
typedef void (*ui_page_create_fn)(lv_obj_t *parent);

/**
 * @brief 页面事件处理函数类型
 * @param event_bits 当前触发的事件位
 */
typedef void (*ui_page_event_fn)(EventBits_t event_bits);

/**
 * @brief 页面销毁函数类型
 * @note 在页面切换前调用，释放页面持有的资源（如 GIF 播放器）
 */
typedef void (*ui_page_destroy_fn)(void);

/**
 * @brief 页面接口结构体
 */
typedef struct {
    ui_page_create_fn  create;    /* 在 parent 下创建页面组件 */
    ui_page_event_fn   on_event;  /* 页面级事件处理（可为 NULL） */
    ui_page_destroy_fn destroy;   /* 页面销毁回调（可为 NULL） */
} ui_page_interface_t;

/* ============================================================
 * 公开 API
 * ============================================================ */

/**
 * @brief 初始化UI管理器
 * @note 创建三层容器（系统层 + 状态栏 + 页面层），显示初始页面，
 *       注册事件观察者。必须在 app_display_init() 之后调用
 */
void ui_manager_init(void);

/**
 * @brief 切换页面
 * @param page 目标页面 ID
 * @note 清空页面容器并创建新页面，状态栏不受影响
 */
void ui_manager_switch_page(ui_page_id_t page);

/**
 * @brief 获取页面容器
 * @return 页面容器对象，供页面模块创建组件时使用
 */
lv_obj_t *ui_manager_get_page_container(void);

/**
 * @brief 获取当前页面 ID
 * @return 当前页面 ID
 */
ui_page_id_t ui_manager_get_current_page(void);

/**
 * @brief 设置首页表情
 * @param emotion 表情名称（"neutral", "happy" 等，共 21 种）
 * @note 仅在首页时生效，必须由事件处理器在 lvgl_port_lock 内调用
 */
void ui_manager_set_emotion(const char *emotion);

/**
 * @brief 设置首页底部对话字幕
 * @param text 字幕文字（传 NULL 清除）
 * @note 仅在首页时生效，必须由事件处理器在 lvgl_port_lock 内调用
 */
void ui_manager_set_subtitle(const char *text);

#endif /* __UI_MANAGER_H__ */
