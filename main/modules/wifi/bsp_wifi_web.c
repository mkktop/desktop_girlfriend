/*
 * Copyright (c) 2025 mkk。版本号 V1.0，发布日期 20250611。
 * 保留所有权利。
 * 
 * 本软件及其相关文档受版权法保护，未经版权所有者明确书面许可，
 * 不得复制、修改、分发、出租、转售或进行任何形式的商业使用。
 * 
 * 如有任何疑问或需要使用授权，请联系 [2864078813@qq.com]。
 * 
 * 使用本模块需要在main文件夹下创建html文件夹，
 * 并将index.html文件放入其中。
 * 请确保文件路径正确，否则可能导致程序无法正常运行。
 * 最后，需要修改CMakeLists.txt文件，加上   EMBED_FILES "html/index.html"   
 */
#include "bsp_wifi_web.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "string.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "cJSON.h"
// 定义日志标签
const char *TAG = "bsp_wifi_web";

//是否自动重连
static bool auto_reconnect = false;
static wifi_manage_cb_t status_callback = NULL; // WiFi状态回调函数
static httpd_handle_t server = NULL;            // HTTP服务器句柄

// 嵌入HTML文件到固件
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

// WiFi状态结构体
static struct
{
    bool connecting;//是否处于连接的过程
    char message[128];//状态消息
    bool connected;//是否连接成功
    char ssid[33];//连接的SSID
    char ip[16];//IP地址
} wifi_state = {
    .connecting = false,
    .message = "",
    .connected = false,
    .ssid = "",
    .ip = ""};


/**
 * @brief WiFi事件处理函数，用于处理WiFi相关事件和IP获取事件
 * 
 * 该函数会根据不同的事件类型（WiFi事件或IP事件），
 * 对不同的事件ID进行处理，并更新WiFi状态信息，
 * 同时在必要时调用状态回调函数通知上层应用。
 * 
 * @param arg 传递给事件处理函数的参数，本函数未使用
 * @param event_base 事件的基础类型，如WiFi_EVENT或IP_EVENT
 * @param event_id 具体的事件ID，用于区分不同的事件
 * @param event_data 事件相关的数据指针，根据不同事件类型有不同的结构体
 */    
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    // 处理WiFi相关事件
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_AP_START: // 配网热点启动
            if (status_callback)
                status_callback(WIFI_MANAGE_EVENT_AP_START);
            break;
        case WIFI_EVENT_AP_STOP: // 配网热点停止
            if (status_callback)
                status_callback(WIFI_MANAGE_EVENT_AP_STOP);
            break;
        case WIFI_EVENT_STA_START: // 开始连接到STA
            if (status_callback)
                status_callback(WIFI_MANAGE_EVENT_STA_START);
            break;
        case WIFI_EVENT_STA_CONNECTED: // 连接到STA成功
        {
            wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
            memcpy(wifi_state.ssid, event->ssid, event->ssid_len);
            wifi_state.ssid[event->ssid_len] = '\0';
            wifi_state.connected = true;
            auto_reconnect = true; // 连接成功后启用自动重连
            if (status_callback)
                status_callback(WIFI_MANAGE_EVENT_STA_CONNECTED);
            char temp[64];
            snprintf(temp, sizeof(temp), "%s", wifi_state.ssid);
            snprintf(wifi_state.message, sizeof(wifi_state.message),
                     "已连接到WiFi: %s", temp);
            ESP_LOGI(TAG, "已连接到WiFi: %s", temp);

            break;
        }
        case WIFI_EVENT_STA_DISCONNECTED: // 与目标WiFi的连接断开
        {
            // 将事件数据转换为对应的结构体指针
            wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
            wifi_state.connected = false;
            wifi_state.ssid[0] = '\0';
            wifi_state.ip[0] = '\0';
            if (status_callback)
                status_callback(WIFI_MANAGE_EVENT_STA_DISCONNECTED);
            wifi_state.connecting = false;

            // 添加更详细的错误信息
            const char *reason;
            switch (event->reason)
            {
            case WIFI_REASON_AUTH_EXPIRE:
                reason = "认证过期啦";
                break;
            case WIFI_REASON_AUTH_FAIL:
                reason = "认证失败啦";
                break;
            case WIFI_REASON_NO_AP_FOUND:
                reason = "没有找到AP呀";
                break;
            case WIFI_REASON_ASSOC_FAIL:
                reason = "关联失败啦";
                break;
            case WIFI_REASON_HANDSHAKE_TIMEOUT:
                reason = "握手超时啦";
                break;
            default:
                reason = "可莉不知道哦";
                break;
            }
            snprintf(wifi_state.message, sizeof(wifi_state.message),
                     "WiFi断开连接, %s", reason);
            ESP_LOGW(TAG, "WiFi断开连接，原因: %s (code: %d)", reason, event->reason);


            // 如果启用了自动重连，则尝试重新连接
            if (auto_reconnect)
            {
                ESP_LOGI(TAG, "尝试重新连接...");
                esp_wifi_connect();
            }
            break;
        }
        }
    }
    // 处理IP相关事件，当设备获取到IP地址时触发
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) // 获取IP地址
    {
        // 将事件数据转换为对应的结构体指针        
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        // 将获取到的IP地址格式化后存储到全局状态结构体中
        snprintf(wifi_state.ip, sizeof(wifi_state.ip), IPSTR, IP2STR(&event->ip_info.ip));
        if (status_callback)
            status_callback(WIFI_MANAGE_EVENT_STA_GOT_IP);
        wifi_state.connecting = false;
        char temp[96];
        snprintf(temp, sizeof(temp), "WiFi: %s", wifi_state.ssid);
        // 更新全局状态结构体中的状态消息，包含SSID和IP地址
        snprintf(wifi_state.message, sizeof(wifi_state.message),
                 "%s\nIP: %s", temp, wifi_state.ip);
    }
}

static esp_err_t read_wifi_config(char *ssid, char *password)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    size_t ssid_len;
    size_t pass_len;

    // 打开nvs
    ret = nvs_open("wifi_config", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open nvs");
        return ret;
    }

    // 先获取实际长度
    ret = nvs_get_str(nvs_handle, "ssid", NULL, &ssid_len);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to get ssid length");
        nvs_close(nvs_handle);
        return ret;
    }

    // 读取ssid
    ret = nvs_get_str(nvs_handle, "ssid", ssid, &ssid_len);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read ssid");
        nvs_close(nvs_handle);
        return ret;
    }

    // 先获取密码实际长度
    ret = nvs_get_str(nvs_handle, "password", NULL, &pass_len);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to get password length");
        nvs_close(nvs_handle);
        return ret;
    }

    // 读取密码
    ret = nvs_get_str(nvs_handle, "password", password, &pass_len);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read password");
        nvs_close(nvs_handle);
        return ret;
    }

    // 关闭nvs
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Read WiFi config success - SSID: %s", ssid);
    return ESP_OK;
}

/**
 * @brief 保存WiFi配置到NVS
 *
 * @param ssid WiFi名称
 * @param password WiFi密码
 * @return esp_err_t
 */
static esp_err_t save_wifi_config(const char *ssid, const char *password)
{

    nvs_handle_t nvs_handle;
    esp_err_t ret;

    // 打开NVS命名空间
    ret = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK)
        return ret;

    // 保存SSID
    ret = nvs_set_str(nvs_handle, "ssid", ssid);
    if (ret != ESP_OK)
    {
        nvs_close(nvs_handle);
        return ret;
    }

    // 保存密码
    ret = nvs_set_str(nvs_handle, "password", password);
    if (ret != ESP_OK)
    {
        nvs_close(nvs_handle);
        return ret;
    }

    // 提交更改
    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "保存WiFi配置 - SSID: %s, Password: %s", ssid, password);
    return ret;
}

/**
 * @brief 处理根路径 ("/") 的 HTTP GET 请求，返回嵌入的 HTML 文件内容
 * 
 * 该函数会将响应的内容类型设置为 "text/html"，并将预先嵌入到固件中的
 * HTML 文件内容发送给客户端。
 * 
 * @param req 指向 httpd_req_t 结构体的指针，包含 HTTP 请求的相关信息
 * @return esp_err_t 操作结果，ESP_OK 表示成功，其他值表示失败
 */
static esp_err_t root_handler(httpd_req_t *req)
{
    // 设置 HTTP 响应的内容类型为 HTML
    httpd_resp_set_type(req, "text/html");
    // 发送预先嵌入到固件中的 HTML 文件内容给客户端
    // index_html_start 为 HTML 文件的起始地址，index_html_end 为结束地址
    // 通过计算两者差值得到 HTML 文件的长度
    httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
    return ESP_OK;
}



/**
 * @brief 处理WiFi连接请求
 * 接收JSON格式：{"ssid": "xxx", "password": "xxx"}
 */
static esp_err_t connect_handler(httpd_req_t *req)
{
    // 定义缓冲区，用于存储接收到的HTTP请求数据
    char buf[128];
    // 接收HTTP请求数据，返回接收到的字节数
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    // 如果接收失败或没有接收到数据，返回错误
    if (ret <= 0)
        return ESP_FAIL;
    // 在接收到的数据末尾添加字符串结束符
    buf[ret] = '\0';

    // 解析接收到的JSON数据
    cJSON *root = cJSON_Parse(buf);
    // 如果解析失败，返回错误
    if (!root)
        return ESP_FAIL;

    // 从JSON数据中获取SSID字段的值
    const char *ssid = cJSON_GetObjectItem(root, "ssid")->valuestring;
    // 从JSON数据中获取password字段的值
    const char *password = cJSON_GetObjectItem(root, "password")->valuestring;

    // 保存WiFi配置到非易失性存储中
    if (save_wifi_config(ssid, password) != ESP_OK)
    {
        // 释放JSON对象内存
        cJSON_Delete(root);
        // 向客户端发送保存配置失败的响应
        httpd_resp_send(req, "Failed to save configuration", -1);
        return ESP_FAIL;
    }

    // 配置WiFi连接参数
    wifi_config_t sta_config = {
        .sta = {
            // 设置认证模式为WPA2-PSK
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,  // 支持Protected Management Frames (PMF)
                .required = false // 不强制要求PMF
            },
        },
    };
    // 将SSID复制到STA配置结构体中，确保不会溢出
    strlcpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
    // 将密码复制到STA配置结构体中，确保不会溢出
    strlcpy((char *)sta_config.sta.password, password, sizeof(sta_config.sta.password));

    // 设置连接状态为正在连接
    wifi_state.connecting = true;
    // 新的连接请求时启用自动重连功能
    auto_reconnect = true; 
    // 更新WiFi状态消息，提示正在连接到指定的SSID
    snprintf(wifi_state.message, sizeof(wifi_state.message), "正在连接到 %s...", ssid);

    // 设置STA模式的WiFi配置
    if (esp_wifi_set_config(WIFI_IF_STA, &sta_config) != ESP_OK)
    {
        // 记录设置STA配置失败的日志
        ESP_LOGW(TAG, "设置STA配置失败");
        // 释放JSON对象内存
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    // 尝试连接到指定的WiFi网络
    if (esp_wifi_connect() != ESP_OK)
    {
        // 记录连接到STA失败的日志
        ESP_LOGW(TAG, "连接到STA失败");
        // 释放JSON对象内存
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    // 记录设置STA配置成功的日志
    ESP_LOGI(TAG, "设置STA配置成功");

    // 释放JSON对象内存
    cJSON_Delete(root);
    // 向客户端发送连接成功的响应
    httpd_resp_send(req, "Success! Device is connecting to WiFi...", -1);
    return ESP_OK;
}



/**
 * @brief 处理WiFi扫描请求的HTTP处理函数
 * 
 * 该函数会触发WiFi扫描操作，获取扫描到的AP（接入点）信息，
 * 并将这些信息以JSON格式返回给客户端。
 * 
 * @param req 指向httpd_req_t结构体的指针，包含HTTP请求的相关信息
 * @return esp_err_t 操作结果，ESP_OK表示成功，其他值表示失败
 */
static esp_err_t scan_handler(httpd_req_t *req)
{
    // 定义WiFi扫描配置结构体，并初始化参数
    // ssid设为NULL表示扫描所有SSID
    // bssid设为NULL表示扫描所有BSSID
    // channel设为0表示扫描所有信道
    // show_hidden设为false表示不显示隐藏的WiFi网络
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false
    };

    // 开始WiFi扫描，第二个参数为true表示阻塞等待扫描完成
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    // 用于存储扫描到的AP数量
    uint16_t ap_count = 0;
    // 获取扫描到的AP数量
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

    // 动态分配内存，用于存储所有扫描到的AP记录
    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    // 检查内存分配是否成功
    if (ap_records == NULL)
    {
        // 内存分配失败，向客户端发送500内部服务器错误响应
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    // 获取所有扫描到的AP记录
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));

    // 创建一个JSON数组，用于存储所有AP信息
    cJSON *root = cJSON_CreateArray();
    // 遍历所有扫描到的AP记录
    for (int i = 0; i < ap_count; i++)
    {
        // 为每个AP创建一个JSON对象
        cJSON *ap = cJSON_CreateObject();
        // 向JSON对象中添加SSID信息
        cJSON_AddStringToObject(ap, "ssid", (char *)ap_records[i].ssid);
        // 向JSON对象中添加信号强度（RSSI）信息
        cJSON_AddNumberToObject(ap, "rssi", ap_records[i].rssi);
        // 向JSON对象中添加是否有密码认证的信息
        cJSON_AddBoolToObject(ap, "auth", ap_records[i].authmode != WIFI_AUTH_OPEN);
        // 将该AP的JSON对象添加到JSON数组中
        cJSON_AddItemToArray(root, ap);
    }

    // 将JSON数组转换为字符串
    char *json_string = cJSON_Print(root);
    // 设置HTTP响应的内容类型为JSON
    httpd_resp_set_type(req, "application/json");
    // 向客户端发送JSON格式的响应
    httpd_resp_send(req, json_string, strlen(json_string));

    // 释放JSON字符串占用的内存
    free(json_string);
    // 删除JSON数组对象，释放相关内存
    cJSON_Delete(root);
    // 释放存储AP记录的内存
    free(ap_records);
    return ESP_OK;
}



/**
 * @brief 处理状态查询请求的HTTP处理函数
 * 
 * 该函数会将当前WiFi的状态信息以JSON格式返回给客户端，
 * 包含连接状态、是否正在连接、状态消息、SSID和IP地址等信息。
 * 
 * @param req 指向httpd_req_t结构体的指针，包含HTTP请求的相关信息
 * @return esp_err_t 操作结果，ESP_OK表示成功，其他值表示失败
 */
static esp_err_t status_handler(httpd_req_t *req)
{
    // 创建一个JSON对象，用于存储WiFi状态信息
    cJSON *root = cJSON_CreateObject();
    // 向JSON对象中添加是否正在连接的布尔值
    cJSON_AddBoolToObject(root, "connecting", wifi_state.connecting);
    // 向JSON对象中添加是否已连接的布尔值
    cJSON_AddBoolToObject(root, "connected", wifi_state.connected);
    // 向JSON对象中添加状态消息字符串
    cJSON_AddStringToObject(root, "message", wifi_state.message);
    // 向JSON对象中添加当前连接的SSID字符串
    cJSON_AddStringToObject(root, "ssid", wifi_state.ssid);
    // 向JSON对象中添加当前分配的IP地址字符串
    cJSON_AddStringToObject(root, "ip", wifi_state.ip);

    // 将JSON对象转换为字符串
    char *json_string = cJSON_Print(root);
    // 设置HTTP响应的内容类型为JSON
    httpd_resp_set_type(req, "application/json");
    // 向客户端发送JSON格式的响应，响应长度为JSON字符串的长度
    httpd_resp_send(req, json_string, strlen(json_string));

    // 释放JSON字符串占用的内存
    free(json_string);
    // 删除JSON对象，释放相关内存
    cJSON_Delete(root);
    return ESP_OK;
}


/**
 * @brief 处理WiFi断开连接请求的HTTP处理函数
 * 
 * 该函数会响应客户端的断开连接请求，禁用自动重连功能，
 * 然后断开当前的WiFi连接，并向客户端发送断开成功的响应。
 * 
 * @param req 指向httpd_req_t结构体的指针，包含HTTP请求的相关信息
 * @return esp_err_t 操作结果，ESP_OK表示成功，其他值表示失败
 */
static esp_err_t disconnect_handler(httpd_req_t *req)
{
    // 手动断开WiFi连接时，禁用自动重连功能，避免设备自动重新连接
    auto_reconnect = false; 
    // 调用ESP-IDF的API断开当前的WiFi连接，若操作失败会触发错误检查
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    // 向客户端发送断开连接成功的响应，响应内容为 "Disconnected"
    httpd_resp_send(req, "Disconnected", -1);
    return ESP_OK;
}



// 添加断开连接的URI处理
static const httpd_uri_t disconnect = {
    .uri = "/disconnect",
    .method = HTTP_POST,
    .handler = disconnect_handler,
    .user_ctx = NULL};

// HTTP服务器URI处理配置
static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
    .user_ctx = NULL};

static const httpd_uri_t connect = {
    .uri = "/connect",
    .method = HTTP_POST,
    .handler = connect_handler,
    .user_ctx = NULL};

static const httpd_uri_t scan = {
    .uri = "/scan",
    .method = HTTP_GET,
    .handler = scan_handler,
    .user_ctx = NULL};

static const httpd_uri_t status = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = status_handler,
    .user_ctx = NULL};

esp_err_t start_webserver(void)
{
    // 创建HTTP服务器
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // 修改HTTP服务器配置
    config.uri_match_fn = httpd_uri_match_wildcard; // 使用通配符匹配URI
    config.max_uri_handlers = 8;                    // 最大URI处理数
    config.max_resp_headers = 8;                    // 最大响应头数
    config.recv_wait_timeout = 30;                  // 接收等待超时时间
    config.send_wait_timeout = 30;                  // 发送等待超时时间
    config.lru_purge_enable = true;                 // 启用LRU清除

    // 启动HTTP服务器
    if (httpd_start(&server, &config) != ESP_OK)
    {
        return ESP_FAIL;
    }

    // 注册URI处理函数
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &connect);
    httpd_register_uri_handler(server, &scan);
    httpd_register_uri_handler(server, &status);
    httpd_register_uri_handler(server, &disconnect);
    return ESP_OK;
}


esp_err_t bsp_wifi_web_init(void)
{
    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    // 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 创建默认的AP和STA网络接口
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));    
    // 注册WiFi事件处理函数
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));  
    return ESP_OK;  
}

esp_err_t bsp_wifi_web_start(void)
{
    //
    char sta_ssid[32];
    char sta_password[64];

    // 设置为AP+STA模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));   
    // 配置AP模式参数
    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .password = WIFI_AP_PASS,
            .channel = WIFI_AP_CHANNEL,
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };     

    // 设置AP配置
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    // 启动WiFi
    ESP_ERROR_CHECK(esp_wifi_start());  
    // 等待WiFi启动
    vTaskDelay(pdMS_TO_TICKS(500)); 

    //尝试读取WiFi配置   
    if (read_wifi_config(sta_ssid, sta_password) == ESP_OK)
    {
        ESP_LOGI(TAG, "WiFi配置已读取: %s, %s", sta_ssid, sta_password);
        // 配置STA配置
        wifi_config_t sta_config = {0}; 

        // 复制SSID和密码
        memcpy(sta_config.sta.ssid, sta_ssid, strlen(sta_ssid));
        memcpy(sta_config.sta.password, sta_password, strlen(sta_password));

        // 设置认证模式
        sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        sta_config.sta.pmf_cfg.capable = true;
        sta_config.sta.pmf_cfg.required = false;

        ESP_LOGI(TAG, "正在连接到WiFi: %s", sta_config.sta.ssid);

        // 设置STA配置
        if (esp_wifi_set_config(WIFI_IF_STA, &sta_config) != ESP_OK)
        {
            ESP_LOGE(TAG, "设置STA配置失败");
            return ESP_FAIL;
        }

        // // 启用自动重连
        // auto_reconnect = true;

        // 连接到STA
        if (esp_wifi_connect() != ESP_OK)
        {
            ESP_LOGE(TAG, "连接到STA失败");
            return ESP_FAIL;
        }
    } 
    // 启动Web服务器
    ESP_ERROR_CHECK(start_webserver());

    ESP_LOGI(TAG, "WiFi配网已启动，请连接到 %s 进行配置，密码 %s", WIFI_AP_SSID,WIFI_AP_PASS);
    ESP_LOGW(TAG, "请在浏览器中进入:192.168.4.1");
    return ESP_OK;     
}

// 实现断开连接接口
esp_err_t bsp_wifi_web_disconnect(void)
{
    return esp_wifi_disconnect();
}

// 获取WiFi连接状态
uint8_t bsp_wifi_web_get_status(void)
{
    if (wifi_state.connected == true && wifi_state.ip[0] != '\0'){
        return 1;
    }
    return 0; // 未连接
}



/**
 * @brief 获取STA模式下WiFi的MAC地址
 * @param mac 指向一个长度至少为 6 字节的缓冲区，用于存储获取到的MAC地址。
 * @return esp_err_t 操作结果，ESP_OK 表示成功获取MAC地址，其他值表示操作失败。
 */
esp_err_t bsp_wifi_web_get_mac(uint8_t *mac)
{
    // WIFI_IF_STA 表示获取STA模式的MAC地址
    // mac 为存储MAC地址的缓冲区指针
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, mac);
    return ret;
}


/**
 * @brief 关闭HTTP服务器
 * 
 * 该函数会检查HTTP服务器句柄是否有效，若有效则调用 `httpd_stop` 函数关闭服务器，
 * 并将服务器句柄置为 NULL。
 * 
 * @return esp_err_t 操作结果，ESP_OK 表示成功关闭服务器，其他值表示失败。
 */
esp_err_t stop_webserver(void)
{
    if (server != NULL) {
        // 调用 httpd_stop 函数关闭 HTTP 服务器
        httpd_stop(server);
        // 将服务器句柄置为 NULL
        server = NULL;
    }
    return ESP_OK;
}



/**
 * @brief 仅关闭AP模式，保留STA模式
 * 
 * 该函数会将WiFi模式设置为 WIFI_MODE_STA，
 * 并重新启动WiFi，同时进行错误检查确保操作成功。
 * 
 * @return esp_err_t 操作结果，ESP_OK 表示成功，其他值表示失败
 */
esp_err_t bsp_wifi_web_stop_ap(void)
{
    // 设置WiFi模式为STA模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // 重新启动WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    return ESP_OK;
}