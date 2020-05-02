// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_heap.h"
#include "ysw_common.h"
#include "esp_log.h"

#define TAG "YSW_HEAP"

void *ysw_heap_allocate_uninitialized(size_t size)
{
    ESP_LOGD(TAG, "ysw_heap_allocate size=%d", size);
    void *p = malloc(size);
    if (!p) {
        ESP_LOGE(TAG, "malloc failed, size=%d", size);
        abort();
    }
    return p;
}

void *ysw_heap_allocate(size_t size)
{
    void *p = ysw_heap_allocate_uninitialized(size);
    memset(p, 0, size);
    return p;
}

void *ysw_heap_reallocate(void *old_p, size_t size)
{
    ESP_LOGD(TAG, "ysw_heap_reallocate old_p=%p, size=%d", old_p, size);
    void *p = realloc(old_p, size);
    if (!p) {
        ESP_LOGE(TAG, "realloc failed, size=%d", size);
        abort();
    }
    return p;
}

char *ysw_heap_strdup(char *source)
{
    char *target = strdup(source);
    if (!target) {
        ESP_LOGE(TAG, "strdup failed, size=%d", strlen(source));
        abort();
    }
    return target;
}

char *ysw_heap_string_reallocate(char *old_string, const char *new_string)
{
    int length = strlen(new_string) + RFT;
    void *p = ysw_heap_reallocate(old_string, length);
    strcpy(p, new_string);
    return p;
}

void ysw_heap_free(void *p)
{
    ESP_LOGD(TAG, "ysw_heap_free p=%p", p);
    free(p);
}

