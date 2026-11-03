#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#define ESP_LOG_NONE 0
#define ESP_LOG_ERROR 1
#define ESP_LOG_WARN 2
#define ESP_LOG_INFO 3
#define ESP_LOG_DEBUG 4
#define ESP_LOG_VERBOSE 5
typedef int esp_log_level_t;
#define ESP_LOG_NOP(tag, fmt, ...) do { (void)(tag); if (0) printf(fmt, ##__VA_ARGS__); } while (0)
#define ESP_LOGE(tag, fmt, ...) ESP_LOG_NOP(tag, fmt, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) ESP_LOG_NOP(tag, fmt, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) ESP_LOG_NOP(tag, fmt, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) ESP_LOG_NOP(tag, fmt, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) ESP_LOG_NOP(tag, fmt, ##__VA_ARGS__)
#define ESP_EARLY_LOGE ESP_LOGE
#define ESP_EARLY_LOGW ESP_LOGW
#define ESP_EARLY_LOGI ESP_LOGI
#define ESP_EARLY_LOGD ESP_LOGD
#define ESP_EARLY_LOGV ESP_LOGV
#define ESP_DRAM_LOGE ESP_LOGE
#define ESP_DRAM_LOGW ESP_LOGW
#define ESP_DRAM_LOGI ESP_LOGI
#define ESP_LOG_LEVEL(l, tag, fmt, ...) ESP_LOG_NOP(tag, fmt, ##__VA_ARGS__)
#define ESP_LOG_LEVEL_LOCAL(l, tag, fmt, ...) ESP_LOG_NOP(tag, fmt, ##__VA_ARGS__)
#define ESP_LOG_BUFFER_HEX(tag, b, l) do { (void)(tag); (void)(b); (void)(l); } while (0)
#define ESP_LOG_BUFFER_HEX_LEVEL(tag, b, l, lv) do { (void)(tag); (void)(b); (void)(l); } while (0)
#define ESP_LOG_BUFFER_HEXDUMP(tag, b, l, lv) do { (void)(tag); (void)(b); (void)(l); } while (0)
static inline void esp_log_level_set(const char *t, esp_log_level_t l) { (void)t; (void)l; }
