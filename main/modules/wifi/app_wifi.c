/**
 * @file app_wifi.c
 * @brief WiFi配网模块实现
 * @author mkk
 * @date 2026-05-30
 * @note 提供WiFi AP+STA模式配网功能，内置HTTP服务器，
 *       通过app_event通知WiFi状态变化
 */

#include "app_wifi.h"
#include "app_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "string.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "cJSON.h"

static const char *TAG = "app_wifi";

/* 自动重连标志 */
static bool auto_reconnect = false;

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
            app_event_send(APP_EVENT_AP_START, NULL);
            break;
        case WIFI_EVENT_AP_STOP:
            app_event_send(APP_EVENT_AP_STOP, NULL);
            break;
        case WIFI_EVENT_STA_START:
            app_event_send(APP_EVENT_STA_START, NULL);
            break;
        case WIFI_EVENT_STA_CONNECTED: {
            wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
            memcpy(wifi_state.ssid, event->ssid, event->ssid_len);
            wifi_state.ssid[event->ssid_len] = '\0';
            wifi_state.connected = true;
            auto_reconnect = true;
            snprintf(wifi_state.message, sizeof(wifi_state.message),
                     "已连接到WiFi: %s", wifi_state.ssid);
            ESP_LOGI(TAG, "已连接到WiFi: %s", wifi_state.ssid);
            app_event_send(APP_EVENT_WIFI_CONNECTED, wifi_state.ssid);
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

            app_event_send(APP_EVENT_WIFI_DISCONNECTED, NULL);

            if (auto_reconnect) {
                ESP_LOGI(TAG, "尝试重新连接...");
                esp_wifi_connect();
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
        char temp[96];
        snprintf(temp, sizeof(temp), "WiFi: %s", wifi_state.ssid);
        snprintf(wifi_state.message, sizeof(wifi_state.message),
                 "%s\nIP: %s", temp, wifi_state.ip);
        app_event_send(APP_EVENT_WIFI_GOT_IP, wifi_state.ip);
    }
}

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
        ESP_LOGE(TAG, "Failed to open nvs");
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
    ESP_LOGI(TAG, "Read WiFi config - SSID: %s", ssid);
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
    if (!root)
        return ESP_FAIL;

    const char *ssid = cJSON_GetObjectItem(root, "ssid")->valuestring;
    const char *password = cJSON_GetObjectItem(root, "password")->valuestring;

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
    auto_reconnect = true;
    snprintf(wifi_state.message, sizeof(wifi_state.message), "正在连接到 %s...", ssid);

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

    cJSON_Delete(root);
    httpd_resp_send(req, "Success! Device is connecting to WiFi...", -1);
    return ESP_OK;
}

static esp_err_t scan_handler(httpd_req_t *req)
{
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
    auto_reconnect = false;
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
    return ESP_OK;
}

esp_err_t app_wifi_start(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .password = WIFI_AP_PASS,
            .channel = WIFI_AP_CHANNEL,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = WIFI_AP_MAX_CONN,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(start_webserver());
    ESP_LOGI(TAG, "WiFi started, AP: %s", WIFI_AP_SSID);
    return ESP_OK;
}

esp_err_t app_wifi_disconnect(void)
{
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
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    return ESP_OK;
}
