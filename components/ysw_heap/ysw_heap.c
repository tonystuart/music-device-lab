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
#include "stdlib.h"

#define TAG "YSW_HEAP"
#define YSW_HEAP_TRACE 1

#ifdef IDF_VER

#include "esp_heap_caps.h"

static void *xmalloc(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}
static void *xrealloc(void *p, size_t size)
{
    return heap_caps_realloc(p, size, MALLOC_CAP_SPIRAM);
}
static void xfree(void *p)
{
    return heap_caps_free(p);
}

#else

static void* xmalloc(size_t size)
{
    return malloc(size);
}
static void *xrealloc(void *p, size_t size)
{
    return realloc(p, size);
}
static void xfree(void *p)
{
    return free(p);
}

#endif

#ifdef YSW_HEAP_TRACE

#define TRACE_TAG "TRACE_HEAP"

static int total_size;
static int maximum_size;

static void* trace_malloc(size_t size)
{
    total_size += size;
    maximum_size = max(total_size, maximum_size);
    ESP_LOGD(TRACE_TAG, "trace_malloc size=%d/%d/%d", size, total_size, maximum_size);
    size_t *ps = xmalloc(size + sizeof(size_t));
    if (!ps) {
        ESP_LOGE(TRACE_TAG, "trace_malloc xmalloc failed, size=%d", size);
        abort();
    }
    *ps++ = size;
    return ps;
}

static void* trace_realloc(void *p, size_t size)
{
    size_t *ps = NULL;
    if (p) {
        ps = (size_t *)p;
        total_size -= *--ps;
        total_size += size;
        maximum_size = max(total_size, maximum_size);
        ESP_LOGD(TRACE_TAG, "trace_realloc size=%d/%d/%d (old_size=%d)", size, total_size, maximum_size, *ps);
        ps = xrealloc(ps, size);
        if (!p) {
            ESP_LOGE(TRACE_TAG, "trace_realloc xrealloc failed, size=%d", size);
            abort();
        }
        *ps++ = size;
    } else {
        ps = trace_malloc(size);
    }
    return ps;
}

static void trace_free(void *p)
{
    size_t *ps = (size_t *)p;
    total_size -= *--ps;
    ESP_LOGD(TRACE_TAG, "trace_free size=%d/%d/%d", *ps, total_size, maximum_size);
    xfree(ps);
}

#define local_malloc trace_malloc
#define local_realloc trace_realloc
#define local_free trace_free

#else

#define local_malloc xmalloc
#define local_realloc xrealloc
#define local_free xfree

#endif

void *ysw_heap_allocate_uninitialized(size_t size)
{
    void *p = local_malloc(size);
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
    void *p = local_realloc(old_p, size);
    if (!p) {
        ESP_LOGE(TAG, "realloc failed, size=%d", size);
        abort();
    }
    return p;
}

char *ysw_heap_strdup(char *source)
{
    int size = strlen(source) + RFT;
    char *target = ysw_heap_allocate_uninitialized(size);
    strcpy(target, source);
    return target;
}

char *ysw_heap_string_reallocate(char *old_string, const char *new_string)
{
    int size = strlen(new_string) + RFT;
    void *p = ysw_heap_reallocate(old_string, size);
    strcpy(p, new_string);
    return p;
}

void ysw_heap_free(void *p)
{
    local_free(p);
}

