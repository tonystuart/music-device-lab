#pragma once

#include "stdint.h"

extern void esp_log(uint8_t, const char *tag, const char *template, ...);

#define ESP_LOGE(tag, format, ...) esp_log(4, tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) esp_log(3, tag, format, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) esp_log(2, tag, format, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) esp_log(1, tag, format, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) esp_log(0, tag, format, ##__VA_ARGS__)
