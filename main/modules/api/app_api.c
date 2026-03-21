/**
 * @file app_api.c
 * @brief HTTP API 调用模块实现
 * @author mkk
 * @date 2026-03-21
 * @note 使用esp_http_client实现HTTP请求
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
 * @brief HTTP事件处理回调
 * @param evt HTTP事件
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    api_response_t *response = (api_response_t *)evt->user_data;
    
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            /* 接收到响应数据 */
            if (response && response->body) {
                /* 追加数据到响应体 */
                strncat(response->body, (char *)evt->data, evt->data_len);
            }
            break;
            
        case HTTP_EVENT_ON_FINISH:
            /* 请求完成 */
            ESP_LOGI(TAG, "HTTP request finished");
            break;
            
        case HTTP_EVENT_DISCONNECTED:
            /* 连接断开 */
            ESP_LOGI(TAG, "HTTP disconnected");
            break;
            
        default:
            break;
    }
    return ESP_OK;
}

void app_api_init(void)
{
    ESP_LOGI(TAG, "API module initialized");
}

int app_api_get(const char *url, api_response_t *response)
{
    if (!url || !response) {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }
    
    /* 分配响应体缓冲区 */
    response->body = (char *)malloc(8192);
    if (!response->body) {
        ESP_LOGE(TAG, "Failed to allocate memory for response");
        return -1;
    }
    response->body[0] = '\0';
    response->body_len = 0;
    
    /* 配置HTTP客户端 */
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
    
    /* 执行GET请求 */
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        /* 获取状态码和响应长度 */
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
    
    esp_http_client_cleanup(client);
    return 0;
}

int app_api_get_with_auth(const char *url, const char *api_key, api_response_t *response)
{
    if (!url || !api_key || !response) {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }
    
    /* 分配响应体缓冲区 */
    response->body = (char *)malloc(8192);
    if (!response->body) {
        ESP_LOGE(TAG, "Failed to allocate memory for response");
        return -1;
    }
    response->body[0] = '\0';
    response->body_len = 0;
    
    /* 配置HTTP客户端 - 使用跳过证书验证 */
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = response,
        .timeout_ms = 10000,
        .buffer_size = 4096,
        .crt_bundle_attach = esp_crt_bundle_attach,  /* 使用CRT bundle */
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(response->body);
        return -1;
    }
    
    /* 设置Authorization header */
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", api_key);
    esp_http_client_set_header(client, "Authorization", auth_header);
    
    /* 执行GET请求 */
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        /* 获取状态码和响应长度 */
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

int app_api_post(const char *url, const char *json_data, api_response_t *response)
{
    if (!url || !json_data || !response) {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }
    
    /* 分配响应体缓冲区 */
    response->body = (char *)malloc(8192);
    if (!response->body) {
        ESP_LOGE(TAG, "Failed to allocate memory for response");
        return -1;
    }
    response->body[0] = '\0';
    response->body_len = 0;
    
    /* 配置HTTP客户端 */
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
    
    /* 设置POST方法和头部 */
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_data, strlen(json_data));
    
    /* 执行POST请求 */
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        /* 获取状态码和响应长度 */
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

void app_api_free_response(api_response_t *response)
{
    if (response && response->body) {
        free(response->body);
        response->body = NULL;
        response->body_len = 0;
    }
}

int app_api_parse_quota(const char *json_str, quota_info_t *quota)
{
    if (!json_str || !quota) {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }
    
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return -1;
    }
    
    /* 初始化配额信息 */
    memset(quota, 0, sizeof(quota_info_t));
    
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!data) {
        ESP_LOGE(TAG, "No data field");
        cJSON_Delete(root);
        return -1;
    }
    
    cJSON *limits = cJSON_GetObjectItem(data, "limits");
    if (!limits) {
        ESP_LOGE(TAG, "No limits field");
        cJSON_Delete(root);
        return -1;
    }
    
    /* 遍历limits数组 */
    cJSON *limit = NULL;
    cJSON_ArrayForEach(limit, limits) {
        cJSON *type = cJSON_GetObjectItem(limit, "type");
        if (!type) continue;
        
        const char *type_str = type->valuestring;
        
        if (strcmp(type_str, "TIME_LIMIT") == 0) {
            /* 解析时间配额 */
            quota->time_usage = cJSON_GetObjectItem(limit, "usage")->valueint;
            quota->time_remaining = cJSON_GetObjectItem(limit, "remaining")->valueint;
            quota->time_percentage = cJSON_GetObjectItem(limit, "percentage")->valueint;
            quota->next_reset_time = cJSON_GetObjectItem(limit, "nextResetTime")->valuedouble;
        } else if (strcmp(type_str, "TOKENS_LIMIT") == 0) {
            /* 解析Token配额 */
            quota->token_percentage = cJSON_GetObjectItem(limit, "percentage")->valueint;
        }
    }
    
    cJSON_Delete(root);
    return 0;
}

int app_api_get_quota(const char *api_key, quota_info_t *quota)
{
    if (!api_key || !quota) {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }
    
    const char *api_url = "https://bigmodel.cn/api/monitor/usage/quota/limit";
    api_response_t response;
    
    if (app_api_get_with_auth(api_url, api_key, &response) != 0) {
        ESP_LOGE(TAG, "Failed to get quota");
        return -1;
    }
    
    int ret = app_api_parse_quota(response.body, quota);
    app_api_free_response(&response);
    
    return ret;
}
