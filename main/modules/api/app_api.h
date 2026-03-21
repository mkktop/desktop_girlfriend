/**
 * @file app_api.h
 * @brief HTTP API 调用模块头文件
 * @author mkk
 * @date 2026-03-21
 * @note 提供HTTP GET/POST请求功能，用于调用外部API
 */

#ifndef __APP_API_H__
#define __APP_API_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief API响应结构体
 */
typedef struct {
    int status_code;           /* HTTP状态码 */
    char *body;              /* 响应体数据 */
    size_t body_len;         /* 响应体长度 */
} api_response_t;

/**
 * @brief 配额信息结构体
 */
typedef struct {
    int time_usage;           /* 时间配额使用次数 */
    int time_remaining;       /* 时间配额剩余次数 */
    int time_percentage;      /* 时间配额使用百分比 */
    long long next_reset_time; /* 下次重置时间戳(毫秒) */
    int token_percentage;     /* Token配额使用百分比 */
} quota_info_t;

/**
 * @brief 初始化API模块
 * @note 必须在使用API功能前调用
 */
void app_api_init(void);

/**
 * @brief 发送HTTP GET请求
 * @param url 请求的完整URL
 * @param response 响应结构体指针，用于接收返回数据
 * @return 0成功，-1失败
 * @note response.body需要调用者释放内存
 */
int app_api_get(const char *url, api_response_t *response);

/**
 * @brief 发送带Authorization的HTTP GET请求
 * @param url 请求的完整URL
 * @param api_key API密钥，用于Authorization header
 * @param response 响应结构体指针，用于接收返回数据
 * @return 0成功，-1失败
 * @note response.body需要调用者释放内存
 */
int app_api_get_with_auth(const char *url, const char *api_key, api_response_t *response);

/**
 * @brief 发送HTTP POST请求
 * @param url 请求的完整URL
 * @param json_data JSON格式的请求数据
 * @param response 响应结构体指针，用于接收返回数据
 * @return 0成功，-1失败
 * @note response.body需要调用者释放内存
 */
int app_api_post(const char *url, const char *json_data, api_response_t *response);

/**
 * @brief 释放API响应内存
 * @param response 响应结构体指针
 */
void app_api_free_response(api_response_t *response);

/**
 * @brief 解析配额信息
 * @param json_str JSON字符串
 * @param quota 配额信息结构体指针
 * @return 0成功，-1失败
 */
int app_api_parse_quota(const char *json_str, quota_info_t *quota);

/**
 * @brief 获取配额信息
 * @param api_key API密钥
 * @param quota 配额信息结构体指针
 * @return 0成功，-1失败
 */
int app_api_get_quota(const char *api_key, quota_info_t *quota);

#endif /* __APP_API_H__ */
