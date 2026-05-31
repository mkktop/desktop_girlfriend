/**
 * @file app_wifi.c
 * @brief WiFi配网模块实现
 * @author mkk
 * @date 2026-05-31
 * @note WiFi配网与重连策略：
 *       首次启动：先尝试连接已保存WiFi，60秒超时后自动进入配网模式，最多立即重试5次；
 *       配网成功后断线：无限重试 + 指数退避（10s→20s→40s→...→300s封顶），不进配网模式；
 *       配网通过 AP 热点 + HTTP 服务器实现
 */

#include "app_wifi.h"
#include "app_event.h"
#include "board.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "string.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "cJSON.h"

#define TAG "app_wifi"

/* 重连和超时常量 */
#define MAX_RECONNECT_COUNT      5       /* 首次连接最大立即重试次数 */
#define CONNECT_TIMEOUT_SEC      60      /* 首次连接超时（秒） */
#define RECONNECT_MIN_INTERVAL_MS  10000   /* 退避最小间隔 10秒 */
#define RECONNECT_MAX_INTERVAL_MS  300000  /* 退避最大间隔 300秒（5分钟） */

/* 模块状态 */
static int s_reconnect_count = 0;
static bool s_config_mode = false;
static bool s_was_connected = false;         /* 是否曾经成功连接过 */
static uint32_t s_reconnect_interval_ms = RECONNECT_MIN_INTERVAL_MS;
static esp_timer_handle_t s_connect_timer;   /* 首次连接60秒超时 */
static esp_timer_handle_t s_reconnect_timer; /* 退避重连定时器 */
static char s_ap_ssid[33] = {0};            /* 生成的AP热点名称 */

/* HTTP服务器句柄 */
static httpd_handle_t server = NULL;

/* 嵌入HTML文件到固件 */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

/* WiFi状态 */
static struct {
    bool connecting;
    char message[128];
    bool connected;
    char ssid[33];
    char ip[16];
} wifi_state = {
    .connecting = false,
    .message = "",
    .connected = false,
    .ssid = "",
    .ip = "",
};

/* ====== 内部函数声明 ====== */

static void enter_config_mode(void);
static void stop_connect_timer(void);
static void start_connect_timer(void);
static void stop_reconnect_timer(void);
static esp_err_t start_webserver(void);

/**
 * @brief 生成动态AP热点名称（前缀 + MAC后4位）
 * @param buf 输出缓冲区
 * @param buf_len 缓冲区长度
 * @note 格式示例：desktop_girlfriend-A1B2
 */
static void generate_ap_ssid(char *buf, size_t buf_len)
{
    const board_t *board = board_get_instance();
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(buf, buf_len, "%s-%02X%02X", board->wifi_ap.ssid_prefix, mac[4], mac[5]);
}

/* ====== 定时器回调 ====== */

/**
 * @brief 首次连接超时回调（60秒）
 * @note 超时后自动进入配网模式
 */
static void connect_timeout_cb(void *arg)
{
    ESP_LOGW(TAG, "连接超时（%d秒），进入配网模式", CONNECT_TIMEOUT_SEC);
    app_event_set_bits(APP_EVENT_WIFI_DISCONNECTED);
    enter_config_mode();
}

/**
 * @brief 退避重连定时器回调
 * @note 每次触发时尝试重新连接WiFi
 */
static void reconnect_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "退避重连（间隔 %lu ms）...", (unsigned long)s_reconnect_interval_ms);
    esp_wifi_connect();
}

/* ====== 定时器管理 ====== */

static void stop_connect_timer(void)
{
    if (s_connect_timer) {
        esp_timer_stop(s_connect_timer);
    }
}

static void start_connect_timer(void)
{
    stop_connect_timer();
    esp_timer_start_once(s_connect_timer, CONNECT_TIMEOUT_SEC * 1000000ULL);
    ESP_LOGI(TAG, "连接超时定时器已启动（%d秒）", CONNECT_TIMEOUT_SEC);
}

static void stop_reconnect_timer(void)
{
    if (s_reconnect_timer) {
        esp_timer_stop(s_reconnect_timer);
    }
}

/**
 * @brief 启动退避重连定时器
 * @note 每次调用翻倍间隔，封顶 RECONNECT_MAX_INTERVAL_MS
 */
static void start_reconnect_timer(void)
{
    stop_reconnect_timer();
    esp_timer_start_once(s_reconnect_timer, (uint64_t)s_reconnect_interval_ms * 1000ULL);

    /* 翻倍间隔，封顶 */
    s_reconnect_interval_ms *= 2;
    if (s_reconnect_interval_ms > RECONNECT_MAX_INTERVAL_MS) {
        s_reconnect_interval_ms = RECONNECT_MAX_INTERVAL_MS;
    }
}

/* ====== 配网模式管理 ====== */

/**
 * @brief 进入配网模式
 * @note 启动 AP 热点和 HTTP 服务器，等待用户配网
 */
static void enter_config_mode(void)
{
    if (s_config_mode) {
        return;
    }
    s_config_mode = true;

    const board_t *board = board_get_instance();
    const wifi_ap_cfg_t *wifi_ap = &board->wifi_ap;

    /* 生成动态AP名称：前缀-MAC后4位 */
    char ap_ssid[33];
    generate_ap_ssid(ap_ssid, sizeof(ap_ssid));
    strlcpy(s_ap_ssid, ap_ssid, sizeof(s_ap_ssid));

    ESP_LOGI(TAG, "进入配网模式，AP: %s", ap_ssid);

    /* 切换为 APSTA 模式 */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t ap_config = {
        .ap = {
            .ssid_len = strlen(ap_ssid),
            .channel = wifi_ap->channel,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = wifi_ap->max_conn,
        },
    };
    strlcpy((char *)ap_config.ap.ssid, ap_ssid, sizeof(ap_config.ap.ssid));
    strlcpy((char *)ap_config.ap.password, wifi_ap->password, sizeof(ap_config.ap.password));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(start_webserver());

    app_event_set_bits(APP_EVENT_WIFI_CONFIG_ENTER);
}

/**
 * @brief 退出配网模式
 * @note 停止 HTTP 服务器，切换为纯 STA 模式
 */
static void exit_config_mode(void)
{
    if (!s_config_mode) {
        return;
    }
    s_config_mode = false;
    s_ap_ssid[0] = '\0';

    ESP_LOGI(TAG, "退出配网模式");

    /* 停止 HTTP 服务器 */
    if (server) {
        httpd_stop(server);
        server = NULL;
    }

    /* 切换为纯 STA 模式 */
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    app_event_set_bits(APP_EVENT_WIFI_CONFIG_EXIT);
}

/* ====== WiFi 事件处理 ====== */

/**
 * @brief WiFi事件处理函数
 * @param arg 未使用
 * @param event_base 事件基础类型
 * @param event_id 事件ID
 * @param event_data 事件数据
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_AP_START:
            app_event_set_bits(APP_EVENT_WIFI_AP_START);
            break;
        case WIFI_EVENT_AP_STOP:
            app_event_set_bits(APP_EVENT_WIFI_AP_STOP);
            break;
        case WIFI_EVENT_STA_START:
            app_event_set_bits(APP_EVENT_WIFI_STA_START);
            break;
        case WIFI_EVENT_STA_CONNECTED: {
            wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
            memcpy(wifi_state.ssid, event->ssid, event->ssid_len);
            wifi_state.ssid[event->ssid_len] = '\0';
            wifi_state.connected = true;
            s_was_connected = true;
            s_reconnect_count = 0;
            s_reconnect_interval_ms = RECONNECT_MIN_INTERVAL_MS;
            snprintf(wifi_state.message, sizeof(wifi_state.message),
                     "已连接到WiFi: %s", wifi_state.ssid);
            ESP_LOGI(TAG, "已连接到WiFi: %s", wifi_state.ssid);

            /* 连接成功，取消所有定时器 */
            stop_connect_timer();
            stop_reconnect_timer();

            app_event_set_bits(APP_EVENT_WIFI_CONNECTED);
            break;
        }
        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
            wifi_state.connected = false;
            wifi_state.ssid[0] = '\0';
            wifi_state.ip[0] = '\0';
            wifi_state.connecting = false;

            const char *reason;
            switch (event->reason) {
            case WIFI_REASON_AUTH_EXPIRE:
                reason = "认证过期";
                break;
            case WIFI_REASON_AUTH_FAIL:
                reason = "认证失败";
                break;
            case WIFI_REASON_NO_AP_FOUND:
                reason = "未找到AP";
                break;
            case WIFI_REASON_ASSOC_FAIL:
                reason = "关联失败";
                break;
            case WIFI_REASON_HANDSHAKE_TIMEOUT:
                reason = "握手超时";
                break;
            default:
                reason = "未知原因";
                break;
            }
            snprintf(wifi_state.message, sizeof(wifi_state.message),
                     "WiFi断开: %s", reason);
            ESP_LOGW(TAG, "WiFi断开，原因: %s (code: %d)", reason, event->reason);

            app_event_set_bits(APP_EVENT_WIFI_DISCONNECTED);

            if (!s_was_connected) {
                /* 首次连接阶段：有限次立即重试，超限进配网模式 */
                if (s_reconnect_count < MAX_RECONNECT_COUNT) {
                    s_reconnect_count++;
                    ESP_LOGI(TAG, "首次连接重试 (%d/%d)...", s_reconnect_count, MAX_RECONNECT_COUNT);
                    esp_wifi_connect();
                } else {
                    ESP_LOGW(TAG, "首次连接重试次数已达上限 (%d)，进入配网模式", MAX_RECONNECT_COUNT);
                    enter_config_mode();
                }
            } else {
                /* 曾成功连接后断线：无限重试 + 指数退避，不进配网 */
                ESP_LOGI(TAG, "启动退避重连（间隔 %lu ms）...",
                         (unsigned long)s_reconnect_interval_ms);
                start_reconnect_timer();
            }
            break;
        }
        default:
            break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(wifi_state.ip, sizeof(wifi_state.ip), IPSTR, IP2STR(&event->ip_info.ip));
        wifi_state.connecting = false;
        s_reconnect_count = 0;
        s_reconnect_interval_ms = RECONNECT_MIN_INTERVAL_MS;

        /* 连接成功，取消所有定时器 */
        stop_connect_timer();
        stop_reconnect_timer();

        char temp[96];
        snprintf(temp, sizeof(temp), "WiFi: %s", wifi_state.ssid);
        snprintf(wifi_state.message, sizeof(wifi_state.message),
                 "%s\nIP: %s", temp, wifi_state.ip);
        app_event_set_bits(APP_EVENT_WIFI_GOT_IP);

        /* 配网模式下获取到 IP，自动退出配网模式 */
        if (s_config_mode) {
            exit_config_mode();
        }
    }
}

/* ====== NVS 读写 ====== */

/**
 * @brief 从NVS读取WiFi配置
 */
static esp_err_t read_wifi_config(char *ssid, char *password)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    size_t ssid_len;
    size_t pass_len;

    ret = nvs_open("wifi_config", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_get_str(nvs_handle, "ssid", NULL, &ssid_len);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }
    ret = nvs_get_str(nvs_handle, "ssid", ssid, &ssid_len);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_get_str(nvs_handle, "password", NULL, &pass_len);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }
    ret = nvs_get_str(nvs_handle, "password", password, &pass_len);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "读取WiFi配置 - SSID: %s", ssid);
    return ESP_OK;
}

/**
 * @brief 保存WiFi配置到NVS
 */
static esp_err_t save_wifi_config(const char *ssid, const char *password)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;

    ret = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK)
        return ret;

    ret = nvs_set_str(nvs_handle, "ssid", ssid);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_set_str(nvs_handle, "password", password);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "保存WiFi配置 - SSID: %s", ssid);
    return ret;
}

/* ====== HTTP 处理函数 ====== */

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
    return ESP_OK;
}

static esp_err_t connect_handler(httpd_req_t *req)
{
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0)
        return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    /* 空指针安全检查 */
    cJSON *ssid_item = cJSON_GetObjectItem(root, "ssid");
    cJSON *pass_item = cJSON_GetObjectItem(root, "password");
    if (!ssid_item || !pass_item || !ssid_item->valuestring || !pass_item->valuestring) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid or password");
        return ESP_FAIL;
    }

    const char *ssid = ssid_item->valuestring;
    const char *password = pass_item->valuestring;

    if (save_wifi_config(ssid, password) != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_send(req, "Failed to save configuration", -1);
        return ESP_FAIL;
    }

    wifi_config_t sta_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false,
            },
        },
    };
    strlcpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
    strlcpy((char *)sta_config.sta.password, password, sizeof(sta_config.sta.password));

    wifi_state.connecting = true;
    s_reconnect_count = 0;
    s_was_connected = false;
    s_reconnect_interval_ms = RECONNECT_MIN_INTERVAL_MS;
    snprintf(wifi_state.message, sizeof(wifi_state.message), "正在连接到 %s...", ssid);
    app_event_set_bits(APP_EVENT_WIFI_CONNECTING);

    if (esp_wifi_set_config(WIFI_IF_STA, &sta_config) != ESP_OK) {
        ESP_LOGW(TAG, "设置STA配置失败");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    if (esp_wifi_connect() != ESP_OK) {
        ESP_LOGW(TAG, "连接到STA失败");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "设置STA配置成功");

    /* 启动连接超时定时器 */
    start_connect_timer();

    cJSON_Delete(root);
    httpd_resp_send(req, "Success! Device is connecting to WiFi...", -1);
    return ESP_OK;
}

static esp_err_t scan_handler(httpd_req_t *req)
{
    app_event_set_bits(APP_EVENT_WIFI_SCANNING);

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
    };
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_records == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < ap_count; i++) {
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid", (char *)ap_records[i].ssid);
        cJSON_AddNumberToObject(ap, "rssi", ap_records[i].rssi);
        cJSON_AddBoolToObject(ap, "auth", ap_records[i].authmode != WIFI_AUTH_OPEN);
        cJSON_AddItemToArray(root, ap);
    }

    char *json_string = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));

    free(json_string);
    cJSON_Delete(root);
    free(ap_records);
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "connecting", wifi_state.connecting);
    cJSON_AddBoolToObject(root, "connected", wifi_state.connected);
    cJSON_AddStringToObject(root, "message", wifi_state.message);
    cJSON_AddStringToObject(root, "ssid", wifi_state.ssid);
    cJSON_AddStringToObject(root, "ip", wifi_state.ip);

    char *json_string = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));

    free(json_string);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t disconnect_handler(httpd_req_t *req)
{
    s_was_connected = false;
    s_reconnect_count = 0;
    s_reconnect_interval_ms = RECONNECT_MIN_INTERVAL_MS;
    stop_connect_timer();
    stop_reconnect_timer();
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    httpd_resp_send(req, "Disconnected", -1);
    return ESP_OK;
}

/* ====== URI 配置 ====== */

static const httpd_uri_t disconnect_uri = {
    .uri = "/disconnect",
    .method = HTTP_POST,
    .handler = disconnect_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t connect_uri = {
    .uri = "/connect",
    .method = HTTP_POST,
    .handler = connect_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t scan_uri = {
    .uri = "/scan",
    .method = HTTP_GET,
    .handler = scan_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t status_uri = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = status_handler,
    .user_ctx = NULL,
};

/* ====== 公开函数 ====== */

static esp_err_t start_webserver(void)
{
    if (server) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 10;
    config.max_resp_headers = 8;
    config.recv_wait_timeout = 30;
    config.send_wait_timeout = 30;
    config.lru_purge_enable = true;

    if (httpd_start(&server, &config) != ESP_OK) {
        return ESP_FAIL;
    }

    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &connect_uri);
    httpd_register_uri_handler(server, &scan_uri);
    httpd_register_uri_handler(server, &status_uri);
    httpd_register_uri_handler(server, &disconnect_uri);
    return ESP_OK;
}

esp_err_t app_wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                &wifi_event_handler, NULL));

    /* 创建首次连接超时定时器（60秒） */
    const esp_timer_create_args_t connect_timer_args = {
        .callback = connect_timeout_cb,
        .name = "connect_timeout",
    };
    ESP_ERROR_CHECK(esp_timer_create(&connect_timer_args, &s_connect_timer));

    /* 创建退避重连定时器 */
    const esp_timer_create_args_t reconnect_timer_args = {
        .callback = reconnect_timer_cb,
        .name = "reconnect_backoff",
    };
    ESP_ERROR_CHECK(esp_timer_create(&reconnect_timer_args, &s_reconnect_timer));

    return ESP_OK;
}

esp_err_t app_wifi_start(void)
{
    char ssid[33] = {0};
    char password[65] = {0};

    /* 尝试读取已保存的WiFi配置 */
    esp_err_t ret = read_wifi_config(ssid, password);

    if (ret == ESP_OK) {
        /* 有保存的配置，先尝试连接 */
        ESP_LOGI(TAG, "发现已保存的WiFi配置，尝试连接: %s", ssid);

        wifi_config_t sta_config = {
            .sta = {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .pmf_cfg = {
                    .capable = true,
                    .required = false,
                },
            },
        };
        strlcpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
        strlcpy((char *)sta_config.sta.password, password, sizeof(sta_config.sta.password));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_connect());

        wifi_state.connecting = true;
        snprintf(wifi_state.message, sizeof(wifi_state.message),
                 "正在连接到 %s...", ssid);
        app_event_set_bits(APP_EVENT_WIFI_CONNECTING);

        /* 启动60秒连接超时定时器 */
        start_connect_timer();
    } else {
        /* 无保存的配置，直接进入配网模式 */
        ESP_LOGI(TAG, "无已保存的WiFi配置，进入配网模式");
        enter_config_mode();
    }

    return ESP_OK;
}

esp_err_t app_wifi_disconnect(void)
{
    s_was_connected = false;
    s_reconnect_count = 0;
    s_reconnect_interval_ms = RECONNECT_MIN_INTERVAL_MS;
    stop_connect_timer();
    stop_reconnect_timer();
    return esp_wifi_disconnect();
}

uint8_t app_wifi_get_status(void)
{
    return wifi_state.connected ? 1 : 0;
}

esp_err_t app_wifi_get_mac(uint8_t *mac)
{
    return esp_wifi_get_mac(WIFI_IF_STA, mac);
}

esp_err_t app_wifi_stop_ap(void)
{
    exit_config_mode();
    return ESP_OK;
}

const char *app_wifi_get_ip(void)
{
    return wifi_state.ip[0] ? wifi_state.ip : "0.0.0.0";
}

const char *app_wifi_get_ssid(void)
{
    return wifi_state.ssid[0] ? wifi_state.ssid : "";
}

const char *app_wifi_get_ap_ssid(void)
{
    return s_ap_ssid[0] ? s_ap_ssid : "";
}

const char *app_wifi_get_ap_password(void)
{
    const board_t *board = board_get_instance();
    return board->wifi_ap.password;
}
