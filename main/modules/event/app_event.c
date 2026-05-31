/**
 * @file app_event.c
 * @brief 事件系统实现
 * @author mkk
 * @date 2026-05-30
 * @note 基于 FreeRTOS EventGroup 的事件驱动架构，
 *       支持观察者模式和延迟回调（Schedule）机制，
 *       所有事件处理在主线程上下文执行，保证线程安全
 */

#include "app_event.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#define TAG "app_event"

/* 配置常量 */
#define APP_MAX_HANDLERS        16
#define APP_SCHEDULE_QUEUE_SIZE 16

/* 延迟回调队列条目 */
typedef struct {
    app_schedule_fn_t fn;
    void             *arg;
} schedule_entry_t;

/* 观察者条目 */
typedef struct {
    app_event_handler_t handler;
    EventBits_t         event_bits;
    void               *user_ctx;
} observer_entry_t;

/* 模块状态 */
static struct {
    EventGroupHandle_t  event_group;
    SemaphoreHandle_t   mutex;
    observer_entry_t    handlers[APP_MAX_HANDLERS];

    /* 延迟回调环形缓冲区 */
    schedule_entry_t    schedule_queue[APP_SCHEDULE_QUEUE_SIZE];
    int                 schedule_head;
    int                 schedule_tail;
    int                 schedule_count;
} s_ctx;

/* 事件位名称表（用于日志） */
static const char *s_bit_names[] = {
    [0]  = "WIFI_CONNECTED",
    [1]  = "WIFI_DISCONNECTED",
    [2]  = "WIFI_GOT_IP",
    [3]  = "WIFI_AP_START",
    [4]  = "WIFI_AP_STOP",
    [5]  = "WIFI_STA_START",
    [6]  = "WIFI_SCANNING",
    [7]  = "WIFI_CONNECTING",
    [8]  = "WIFI_CONFIG_ENTER",
    [9]  = "WIFI_CONFIG_EXIT",
    [23] = "SCHEDULE_PENDING",
};

/**
 * @brief 打印事件位日志
 */
static void log_event_bits(EventBits_t bits)
{
    if (bits == 0) return;

    bool first = true;
    ESP_LOGI(TAG, "Event bits: 0x%06lX (", (unsigned long)bits);
    for (int i = 0; i < 24; i++) {
        if (bits & (1U << i)) {
            const char *name = (i < (int)(sizeof(s_bit_names) / sizeof(s_bit_names[0])))
                               ? s_bit_names[i] : NULL;
            if (name) {
                ESP_LOGI(TAG, "  %s%s", first ? "" : ", ", name);
            } else {
                ESP_LOGI(TAG, "  %sBIT%d", first ? "" : ", ", i);
            }
            first = false;
        }
    }
    ESP_LOGI(TAG, ")");
}

void app_event_init(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));

    s_ctx.event_group = xEventGroupCreate();
    s_ctx.mutex = xSemaphoreCreateMutex();

    ESP_LOGI(TAG, "Event system initialized (EventGroup + observer + schedule)");
}

EventBits_t app_event_wait(EventBits_t bits_to_wait, bool clear_on_exit, uint32_t timeout_ms)
{
    return xEventGroupWaitBits(
        s_ctx.event_group,
        bits_to_wait,
        clear_on_exit ? pdTRUE : pdFALSE,
        pdFALSE,       /* 等待任意一个位 */
        pdMS_TO_TICKS(timeout_ms)
    );
}

void app_event_set_bits(EventBits_t bits)
{
    log_event_bits(bits);
    xEventGroupSetBits(s_ctx.event_group, bits);
}

int app_event_register_handler(app_event_handler_t handler, EventBits_t event_bits, void *user_ctx)
{
    if (!handler || !event_bits) {
        ESP_LOGE(TAG, "Invalid handler or event_bits");
        return -1;
    }

    xSemaphoreTake(s_ctx.mutex, portMAX_DELAY);

    /* 检查是否已注册（防重复） */
    for (int i = 0; i < APP_MAX_HANDLERS; i++) {
        if (s_ctx.handlers[i].handler == handler) {
            /* 更新现有注册 */
            s_ctx.handlers[i].event_bits = event_bits;
            s_ctx.handlers[i].user_ctx = user_ctx;
            xSemaphoreGive(s_ctx.mutex);
            ESP_LOGI(TAG, "Handler updated at slot %d", i);
            return 0;
        }
    }

    /* 查找空槽 */
    for (int i = 0; i < APP_MAX_HANDLERS; i++) {
        if (s_ctx.handlers[i].handler == NULL) {
            s_ctx.handlers[i].handler = handler;
            s_ctx.handlers[i].event_bits = event_bits;
            s_ctx.handlers[i].user_ctx = user_ctx;
            xSemaphoreGive(s_ctx.mutex);
            ESP_LOGI(TAG, "Handler registered at slot %d (bits=0x%06lX)",
                     i, (unsigned long)event_bits);
            return 0;
        }
    }

    xSemaphoreGive(s_ctx.mutex);
    ESP_LOGE(TAG, "No free handler slots");
    return -1;
}

void app_event_unregister_handler(app_event_handler_t handler)
{
    if (!handler) return;

    xSemaphoreTake(s_ctx.mutex, portMAX_DELAY);
    for (int i = 0; i < APP_MAX_HANDLERS; i++) {
        if (s_ctx.handlers[i].handler == handler) {
            s_ctx.handlers[i].handler = NULL;
            s_ctx.handlers[i].event_bits = 0;
            s_ctx.handlers[i].user_ctx = NULL;
            xSemaphoreGive(s_ctx.mutex);
            ESP_LOGI(TAG, "Handler unregistered from slot %d", i);
            return;
        }
    }
    xSemaphoreGive(s_ctx.mutex);
}

void app_event_dispatch_to_handlers(EventBits_t bits)
{
    if (bits == 0) return;

    /* 在锁下快照匹配的观察者 */
    typedef struct { app_event_handler_t fn; void *ctx; } dispatch_t;
    dispatch_t to_dispatch[APP_MAX_HANDLERS];
    int count = 0;

    xSemaphoreTake(s_ctx.mutex, portMAX_DELAY);
    for (int i = 0; i < APP_MAX_HANDLERS; i++) {
        if (s_ctx.handlers[i].handler &&
            (s_ctx.handlers[i].event_bits & bits)) {
            to_dispatch[count].fn = s_ctx.handlers[i].handler;
            to_dispatch[count].ctx = s_ctx.handlers[i].user_ctx;
            count++;
        }
    }
    xSemaphoreGive(s_ctx.mutex);

    /* 在锁外执行回调 */
    for (int i = 0; i < count; i++) {
        to_dispatch[i].fn(bits, to_dispatch[i].ctx);
    }
}

int app_event_schedule(app_schedule_fn_t fn, void *arg)
{
    if (!fn) return -1;

    xSemaphoreTake(s_ctx.mutex, portMAX_DELAY);

    if (s_ctx.schedule_count >= APP_SCHEDULE_QUEUE_SIZE) {
        xSemaphoreGive(s_ctx.mutex);
        ESP_LOGW(TAG, "Schedule queue full (%d)", APP_SCHEDULE_QUEUE_SIZE);
        return -1;
    }

    schedule_entry_t *entry = &s_ctx.schedule_queue[s_ctx.schedule_tail];
    entry->fn = fn;
    entry->arg = arg;
    s_ctx.schedule_tail = (s_ctx.schedule_tail + 1) % APP_SCHEDULE_QUEUE_SIZE;
    s_ctx.schedule_count++;

    xSemaphoreGive(s_ctx.mutex);

    /* 唤醒主循环 */
    app_event_set_bits(APP_EVENT_SCHEDULE_PENDING);
    return 0;
}

void app_event_schedule_drain(void)
{
    while (1) {
        schedule_entry_t entry;

        xSemaphoreTake(s_ctx.mutex, portMAX_DELAY);
        if (s_ctx.schedule_count == 0) {
            xSemaphoreGive(s_ctx.mutex);
            return;
        }
        entry = s_ctx.schedule_queue[s_ctx.schedule_head];
        s_ctx.schedule_head = (s_ctx.schedule_head + 1) % APP_SCHEDULE_QUEUE_SIZE;
        s_ctx.schedule_count--;
        xSemaphoreGive(s_ctx.mutex);

        /* 在锁外执行回调 */
        entry.fn(entry.arg);
    }
}
