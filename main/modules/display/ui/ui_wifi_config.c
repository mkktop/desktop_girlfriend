/**
 * @file ui_wifi_config.c
 * @brief WiFi配网引导页面实现
 * @author mkk
 * @date 2026-05-31
 * @note 配网模式下在屏幕上显示引导信息：
 *       WiFi图标 + "配网模式" 标题 + AP热点名称/密码/浏览器URL
 */

#include "ui_wifi_config.h"
#include "app_wifi.h"
#include "app_font.h"
#include "lvgl.h"

/* 页面对象引用（用于 destroy 时清理） */
static lv_obj_t *s_icon_label = NULL;
static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_hint_label = NULL;
static lv_obj_t *s_wait_label = NULL;

/**
 * @brief 创建WiFi配网引导页面
 */
void ui_wifi_config_create(void)
{
    lv_obj_t *scr = lv_screen_active();

    /* WiFi 图标（居中，偏上，使用 Montserrat 40 的 FontAwesome 符号） */
    s_icon_label = lv_label_create(scr);
    lv_label_set_text(s_icon_label, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(s_icon_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(s_icon_label, lv_color_hex(0x4A90D9), 0);
    lv_obj_align(s_icon_label, LV_ALIGN_TOP_MID, 0, 35);

    /* "配网模式" 标题（使用字体管理器提供的 CJK 字体） */
    s_title_label = lv_label_create(scr);
    lv_label_set_text(s_title_label, "\xe9\x85\x8d\xe7\xbd\x91\xe6\xa8\xa1\xe5\xbc\x8f"); /* 配网模式 */
    lv_obj_set_style_text_font(s_title_label, app_font_get_text(), 0);
    lv_obj_align(s_title_label, LV_ALIGN_TOP_MID, 0, 85);

    /* 引导文本：动态拼接AP名称、密码、URL */
    const char *ap_ssid = app_wifi_get_ap_ssid();
    const char *ap_pwd = app_wifi_get_ap_password();

    char hint_text[256];
    snprintf(hint_text, sizeof(hint_text),
        "\xe6\x89\x8b\xe6\x9c\xba\xe8\xbf\x9e\xe6\x8e\xa5WiFi\xe7\x83\xad\xe7\x82\xb9\xef\xbc\x9a\n"  /* 手机连接WiFi热点：\n */
        "%s\n"
        "\xe5\xaf\x86\xe7\xa0\x81\xef\xbc\x9a%s\n"                                              /* 密码：xxx\n */
        "\xe6\xb5\x8f\xe8\xa7\x88\xe5\x99\xa8\xe8\xae\xbf\xe9\x97\xae\xef\xbc\x9a\n"            /* 浏览器访问：\n */
        "http://192.168.4.1",
        ap_ssid, ap_pwd);

    s_hint_label = lv_label_create(scr);
    lv_label_set_text(s_hint_label, hint_text);
    lv_label_set_long_mode(s_hint_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_hint_label, app_font_get_text(), 0);
    lv_obj_set_width(s_hint_label, 220);
    lv_obj_align(s_hint_label, LV_ALIGN_TOP_MID, 0, 115);

    /* 底部等待提示 */
    s_wait_label = lv_label_create(scr);
    lv_label_set_text(s_wait_label, "\xe2\x97\x8f \xe7\xad\x89\xe5\xbe\x85\xe9\x85\x8d\xe7\xbd\x91\xe4\xb8\xad..."); /* ● 等待配网中... */
    lv_obj_set_style_text_font(s_wait_label, app_font_get_text(), 0);
    lv_obj_set_style_text_color(s_wait_label, lv_color_hex(0x999999), 0);
    lv_obj_align(s_wait_label, LV_ALIGN_TOP_MID, 0, 280);
}

/**
 * @brief 销毁WiFi配网引导页面
 */
void ui_wifi_config_destroy(void)
{
    if (s_icon_label) {
        lv_obj_delete(s_icon_label);
        s_icon_label = NULL;
    }
    if (s_title_label) {
        lv_obj_delete(s_title_label);
        s_title_label = NULL;
    }
    if (s_hint_label) {
        lv_obj_delete(s_hint_label);
        s_hint_label = NULL;
    }
    if (s_wait_label) {
        lv_obj_delete(s_wait_label);
        s_wait_label = NULL;
    }
}
