/**
 * @file app_api.c
 * @brief HTTP API 调用模块实现
 * @author mkk
 * @date 2026-03-21
 * @note 使用esp_http_client实现HTTP请求，支持GET/POST方法及Bearer Token认证
 */

#include "app_api.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "string.h"
#include "stdlib.h"

static const char *TAG = "app_api";

/**
 * @brief HTTP事件处理回调函数
 * @param evt HTTP事件结构体指针，包含事件类型和数据
 * @return ESP_OK表示处理成功
 * @note 此回调由esp_http_client在以下时机调用：
 *       - HTTP_EVENT_ON_DATA：每次接收到数据块时触发，数据可能分多次到达
 *       - HTTP_EVENT_ON_FINISH：HTTP请求完成时触发
 *       - HTTP_EVENT_DISCONNECTED：连接断开时触发
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    /* 从user_data获取响应结构体，由调用者在配置中传入 */
    api_response_t *response = (api_response_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            /*
             * 接收到响应数据块，HTTP响应可能分多个数据包到达，
             * 需要将每次接收到的数据追加到response->body缓冲区中
             */
            if (response && response->body) {
                /* 计算当前缓冲区已使用的长度 */
                size_t current_len = strlen(response->body);
                /* 计算剩余可用空间 */
                size_t remaining = 8192 - current_len - 1;  /* -1为保留null终止符 */
                /* 安全追加数据，防止缓冲区溢出 */
                size_t copy_len = (evt->data_len < remaining) ? evt->data_len : remaining;
                if (copy_len > 0) {
                    strncat(response->body, (char *)evt->data, copy_len);
                }
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            /* HTTP请求已完成，所有响应数据已接收完毕 */
            ESP_LOGI(TAG, "HTTP request finished");
            break;

        case HTTP_EVENT_DISCONNECTED:
            /* 网络连接断开，可能是正常结束或异常断开 */
            ESP_LOGI(TAG, "HTTP disconnected");
            break;

        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief 初始化API模块
 * @note 目前仅打印日志，预留用于后续扩展（如初始化TLS、证书等）
 */
void app_api_init(void)
{
    ESP_LOGI(TAG, "API module initialized");
}

/**
 * @brief 发送HTTP GET请求（无认证）
 * @param url 请求的完整URL，如 "http://example.com/api/data"
 * @param response 响应结构体指针，用于接收返回数据
 *                 - 调用前无需初始化body字段
 *                 - 调用后需检查返回值，成功时body需要调用app_api_free_response释放
 * @return 0表示成功，-1表示失败
 * @note 适用于不需要认证的公开API接口
 */
int app_api_get(const char *url, api_response_t *response)
{
    /* 参数有效性检查 */
    if (!url || !response) {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }

    /*
     * 分配响应体缓冲区，8KB足以容纳大多数API响应
     * 注意：调用者负责在完成后释放此内存
     */
    response->body = (char *)malloc(8192);
    if (!response->body) {
        ESP_LOGE(TAG, "Failed to allocate memory for response");
        return -1;
    }
    response->body[0] = '\0';  /* 初始化为空字符串，便于strncat追加 */
    response->body_len = 0;

    /*
     * 配置HTTP客户端参数：
     * - event_handler：数据接收回调，用于收集响应体
     * - user_data：传递response指针给回调函数
     * - timeout_ms：请求超时时间，5秒适用于局域网或响应快的API
     * - buffer_size：HTTP头缓冲区大小，非响应体大小
     */
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = response,
        .timeout_ms = 5000,
        .buffer_size = 1024,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(response->body);
        return -1;
    }

    /* 执行同步GET请求，会阻塞直到收到完整响应或超时 */
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        response->status_code = esp_http_client_get_status_code(client);
        response->body_len = strlen(response->body);
        ESP_LOGI(TAG, "HTTP GET Status = %d, body_len = %d",
                  response->status_code, response->body_len);
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        free(response->body);
        response->body = NULL;
        esp_http_client_cleanup(client);
        return -1;
    }

    /* 释放HTTP客户端资源 */
    esp_http_client_cleanup(client);
    return 0;
}

/**
 * @brief 发送带Bearer Token认证的HTTP GET请求
 * @param url 请求的完整URL，如 "https://bigmodel.cn/api/xxx"
 * @param api_key API密钥，会自动拼接为 "Bearer {api_key}" 格式
 * @param response 响应结构体指针，用于接收返回数据
 * @return 0表示成功，-1表示失败
 * @note 用于调用需要认证的HTTPS API，如BigModel/ChatGLM等
 */
int app_api_get_with_auth(const char *url, const char *api_key, api_response_t *response)
{
    if (!url || !api_key || !response) {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }

    response->body = (char *)malloc(8192);
    if (!response->body) {
        ESP_LOGE(TAG, "Failed to allocate memory for response");
        return -1;
    }
    response->body[0] = '\0';
    response->body_len = 0;

    /*
     * HTTPS配置说明：
     * - crt_bundle_attach：使用ESP-IDF内置的CA证书包，支持大多数主流网站的HTTPS证书
     * - timeout_ms：设置为10秒，HTTPS握手和API响应可能较慢
     * - buffer_size：4KB，用于存储HTTP响应头
     */
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = response,
        .timeout_ms = 10000,
        .buffer_size = 4096,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(response->body);
        return -1;
    }

    /*
     * 设置Authorization请求头，格式为 "Bearer {api_key}"
     * 这是OAuth 2.0和大多数现代API的标准认证方式
     */
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", api_key);
    esp_http_client_set_header(client, "Authorization", auth_header);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        response->status_code = esp_http_client_get_status_code(client);
        response->body_len = strlen(response->body);
        ESP_LOGI(TAG, "HTTP GET with auth Status = %d, body_len = %d",
                  response->status_code, response->body_len);
    } else {
        ESP_LOGE(TAG, "HTTP GET with auth request failed: %s", esp_err_to_name(err));
        free(response->body);
        response->body = NULL;
        esp_http_client_cleanup(client);
        return -1;
    }

    esp_http_client_cleanup(client);
    return 0;
}

/**
 * @brief 发送HTTP POST请求（JSON格式，无认证）
 * @param url 请求的完整URL
 * @param json_data JSON格式的请求体数据，如 "{\"key\":\"value\"}"
 * @param response 响应结构体指针，用于接收返回数据
 * @return 0表示成功，-1表示失败
 * @note 自动设置Content-Type为application/json
 */
int app_api_post(const char *url, const char *json_data, api_response_t *response)
{
    if (!url || !json_data || !response) {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }

    response->body = (char *)malloc(8192);
    if (!response->body) {
        ESP_LOGE(TAG, "Failed to allocate memory for response");
        return -1;
    }
    response->body[0] = '\0';
    response->body_len = 0;

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = response,
        .timeout_ms = 5000,
        .buffer_size = 1024,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(response->body);
        return -1;
    }

    /*
     * POST请求配置：
     * 1. 设置HTTP方法为POST
     * 2. 设置Content-Type为JSON格式
     * 3. 设置请求体数据
     */
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_data, strlen(json_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        response->status_code = esp_http_client_get_status_code(client);
        response->body_len = strlen(response->body);
        ESP_LOGI(TAG, "HTTP POST Status = %d, body_len = %d",
                  response->status_code, response->body_len);
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        free(response->body);
        response->body = NULL;
        esp_http_client_cleanup(client);
        return -1;
    }

    esp_http_client_cleanup(client);
    return 0;
}

/**
 * @brief 释放API响应结构体中的内存
 * @param response 响应结构体指针
 * @note 必须在app_api_get/post函数调用成功后、不再使用response时调用
 *       不会释放response结构体本身，只释放内部的body缓冲区
 */
void app_api_free_response(api_response_t *response)
{
    if (response && response->body) {
        free(response->body);
        response->body = NULL;
        response->body_len = 0;
    }
}

/**
 * @brief 解析BigModel API配额信息的JSON响应
 * @param json_str JSON字符串，来自BigModel配额API的响应体
 * @param quota 配额信息结构体指针，用于存储解析结果
 * @return 0表示成功，-1表示失败
 * @note BigModel配额API返回格式示例：
 *       {
 *         "data": {
 *           "limits": [
 *             {"type": "TIME_LIMIT", "usage": 10, "remaining": 90, "percentage": 10, "nextResetTime": 1711084800000},
 *             {"type": "TOKENS_LIMIT", "percentage": 5}
 *           ]
 *         }
 *       }
 */
int app_api_parse_quota(const char *json_str, quota_info_t *quota)
{
    if (!json_str || !quota) {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }

    /* 解析JSON字符串为cJSON对象 */
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return -1;
    }

    /* 清零配额结构体，确保未解析的字段为0 */
    memset(quota, 0, sizeof(quota_info_t));

    /* 获取data对象 */
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!data) {
        ESP_LOGE(TAG, "No data field");
        cJSON_Delete(root);
        return -1;
    }

    /* 获取limits数组，包含多种类型的配额限制 */
    cJSON *limits = cJSON_GetObjectItem(data, "limits");
    if (!limits) {
        ESP_LOGE(TAG, "No limits field");
        cJSON_Delete(root);
        return -1;
    }

    /*
     * 遍历limits数组，根据type字段区分不同类型的配额：
     * - TIME_LIMIT：时间/次数配额，包含使用次数、剩余次数、百分比、重置时间
     * - TOKENS_LIMIT：Token配额，仅包含使用百分比
     */
    cJSON *limit = NULL;
    cJSON_ArrayForEach(limit, limits) {
        cJSON *type = cJSON_GetObjectItem(limit, "type");
        if (!type) continue;

        const char *type_str = type->valuestring;

        if (strcmp(type_str, "TIME_LIMIT") == 0) {
            /* 解析时间配额（请求次数限制） */
            cJSON *usage = cJSON_GetObjectItem(limit, "usage");
            cJSON *remaining = cJSON_GetObjectItem(limit, "remaining");
            cJSON *percentage = cJSON_GetObjectItem(limit, "percentage");
            cJSON *next_reset = cJSON_GetObjectItem(limit, "nextResetTime");

            if (usage) quota->time_usage = usage->valueint;
            if (remaining) quota->time_remaining = remaining->valueint;
            if (percentage) quota->time_percentage = percentage->valueint;
            if (next_reset) quota->next_reset_time = (long long)next_reset->valuedouble;
        } else if (strcmp(type_str, "TOKENS_LIMIT") == 0) {
            /* 解析Token配额（总Token使用百分比） */
            cJSON *percentage = cJSON_GetObjectItem(limit, "percentage");
            if (percentage) quota->token_percentage = percentage->valueint;
        }
    }

    /* 释放cJSON对象，防止内存泄漏 */
    cJSON_Delete(root);
    return 0;
}

/**
 * @brief 获取BigModel API的配额信息
 * @param api_key BigModel API密钥
 * @param quota 配额信息结构体指针，用于存储返回的配额数据
 * @return 0表示成功，-1表示失败
 * @note 封装了HTTP请求和JSON解析的完整流程
 */
int app_api_get_quota(const char *api_key, quota_info_t *quota)
{
    if (!api_key || !quota) {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }

    /* BigModel配额查询API地址 */
    const char *api_url = "https://bigmodel.cn/api/monitor/usage/quota/limit";
    api_response_t response;

    /* 发送带认证的GET请求 */
    if (app_api_get_with_auth(api_url, api_key, &response) != 0) {
        ESP_LOGE(TAG, "Failed to get quota");
        return -1;
    }

    /* 解析JSON响应 */
    int ret = app_api_parse_quota(response.body, quota);

    /* 释放响应缓冲区 */
    app_api_free_response(&response);

    return ret;
}
