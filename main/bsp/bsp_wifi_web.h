#ifndef __BSP_WIFI_WEB_H__
#define __BSP_WIFI_WEB_H__

#include "esp_err.h"

// WiFi配置参数
#define WIFI_AP_SSID "mkk_wifi" // 配网热点名称
#define WIFI_AP_PASS "123456789"     // 配网热点密码
#define WIFI_AP_CHANNEL 1           // WiFi信道
#define WIFI_CONNECT_TIMEOUT 30000  // 连接超时时间(ms)
#define WIFI_AP_MAX_CONN 1          // 最大连接数

typedef enum {
    WIFI_MANAGE_EVENT_AP_START = 0,        // AP模式启动成功
    WIFI_MANAGE_EVENT_AP_STOP,             // AP模式已停止
    WIFI_MANAGE_EVENT_STA_START,           // Station模式启动成功
    WIFI_MANAGE_EVENT_STA_CONNECTED,       // 已连接到目标WiFi
    WIFI_MANAGE_EVENT_STA_DISCONNECTED,    // 与目标WiFi的连接断开
    WIFI_MANAGE_EVENT_STA_GOT_IP,         // 获取到IP地址
} wifi_manage_event_t;


typedef void (*wifi_manage_cb_t)(wifi_manage_event_t event);
esp_err_t start_webserver(void);
esp_err_t bsp_wifi_web_init(void);
esp_err_t bsp_wifi_web_start(void);
esp_err_t bsp_wifi_web_disconnect(void);
uint8_t bsp_wifi_web_get_status(void);
esp_err_t bsp_wifi_web_get_mac(uint8_t *mac);
esp_err_t stop_webserver(void);
esp_err_t bsp_wifi_web_stop_ap(void);
#endif /* __BSP_WIFI_WEB_H__ */
