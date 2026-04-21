/**
 * @file app_api.h
 * @brief HTTP API 调用模块头文件
 * @author mkk
 * @date 2026-03-21
 * @note 提供HTTP GET/POST请求功能，用于调用外部API。
 *       支持Bearer Token认证，内置BigModel API配额查询功能。
 */

#ifndef __APP_API_H__
#define __APP_API_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief API响应结构体
 * @note 用于接收HTTP响应数据，调用者需在使用后调用app_api_free_response释放内存
 */
typedef struct {
    int status_code;           /* HTTP状态码，如200表示成功，401表示未授权 */
    char *body;                /* 响应体数据，动态分配的字符串，以'\0'结尾 */
    size_t body_len;           /* 响应体实际长度（不含终止符） */
} api_response_t;

/**
 * @brief BigModel API配额信息结构体
 * @note 对应BigModel配额API返回的limits数组中的数据
 */
typedef struct {
    int time_usage;            /* 时间/次数配额：已使用次数 */
    int time_remaining;        /* 时间/次数配额：剩余次数 */
    int time_percentage;       /* 时间/次数配额：使用百分比（0-100） */
    long long next_reset_time; /* 下次配额重置时间戳（毫秒级Unix时间戳） */
    int token_percentage;      /* Token配额：使用百分比（0-100） */
} quota_info_t;

/**
 * @brief 初始化API模块
 * @note 必须在使用其他API功能前调用，目前仅打印日志，预留用于后续扩展
 */
void app_api_init(void);

/**
 * @brief 发送HTTP GET请求（无认证）
 * @param url 请求的完整URL，如 "http://example.com/api/data"
 * @param response 响应结构体指针，用于接收返回数据
 *                 - 调用前无需初始化body字段
 *                 - 调用成功后需调用app_api_free_response释放内存
 * @return 0表示成功，-1表示失败
 * @note 适用于不需要认证的公开API接口，超时时间5秒
 */
int app_api_get(const char *url, api_response_t *response);

/**
 * @brief 发送带Bearer Token认证的HTTP GET请求
 * @param url 请求的完整URL，如 "https://bigmodel.cn/api/xxx"
 * @param api_key API密钥，会自动拼接为 "Bearer {api_key}" 格式放入Authorization头
 * @param response 响应结构体指针，用于接收返回数据
 * @return 0表示成功，-1表示失败
 * @note 支持HTTPS（使用ESP-IDF内置CA证书），超时时间10秒
 */
int app_api_get_with_auth(const char *url, const char *api_key, api_response_t *response);

/**
 * @brief 发送HTTP POST请求（JSON格式，无认证）
 * @param url 请求的完整URL
 * @param json_data JSON格式的请求体数据，如 "{\"key\":\"value\"}"
 * @param response 响应结构体指针，用于接收返回数据
 * @return 0表示成功，-1表示失败
 * @note 自动设置Content-Type为application/json，超时时间5秒
 */
int app_api_post(const char *url, const char *json_data, api_response_t *response);

/**
 * @brief 释放API响应结构体中的内存
 * @param response 响应结构体指针
 * @note 必须在app_api_get/post函数调用成功后、不再使用response时调用
 *       仅释放内部的body缓冲区，不释放response结构体本身
 */
void app_api_free_response(api_response_t *response);

/**
 * @brief 解析BigModel API配额信息的JSON响应
 * @param json_str JSON字符串，来自BigModel配额API的响应体
 * @param quota 配额信息结构体指针，用于存储解析结果
 * @return 0表示成功，-1表示失败（JSON格式错误或缺少必要字段）
 */
int app_api_parse_quota(const char *json_str, quota_info_t *quota);

/**
 * @brief 获取BigModel API的配额信息（便捷函数）
 * @param api_key BigModel API密钥
 * @param quota 配额信息结构体指针，用于存储返回的配额数据
 * @return 0表示成功，-1表示失败
 * @note 封装了HTTP请求和JSON解析的完整流程
 */
int app_api_get_quota(const char *api_key, quota_info_t *quota);

#endif /* __APP_API_H__ */
