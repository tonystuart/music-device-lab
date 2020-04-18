// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_array.h"

#include "assert.h"
#include "esp_log.h"
#include "ysw_heap.h"

#define TAG "YSW_ARRAY"

ysw_array_t *ysw_array_create(size_t initial_size)
{
    ESP_LOGD(TAG, "create initial_size=%d", initial_size);
    ysw_array_t *array = ysw_heap_allocate(sizeof(ysw_array_t));
    array->count = 0;
    array->size = initial_size;
    if (initial_size) {
        array->data = ysw_heap_allocate(sizeof(void*) * initial_size);
    } else {
        array->data = NULL;
    }
    return array;
}

int ysw_array_push(ysw_array_t *array, void *data)
{
    assert(array);
    ESP_LOGD(TAG, "push array=%p, data=%p, count=%d, size=%d", array, data, array->count, array->size);
    int index = array->count++;
    if (array->count > array->size) {
        array->size = array->size ? array->size * 2 : 4;
        ESP_LOGD(TAG, "push resizing to size=%d", array->size);
        array->data = ysw_heap_reallocate(array->data, sizeof(void*) * array->size);
    }
    array->data[index] = data;
    return index;
}

void *ysw_array_pop(ysw_array_t *array)
{
    assert(array);
    assert(array->count);
    ESP_LOGD(TAG, "pop array=%p, count=%d, size=%d", array, array->count, array->size);
    int index = --array->count;
    return array->data[index];
}

void ysw_array_resize(ysw_array_t *array, size_t new_size)
{
    assert(array);
    ESP_LOGD(TAG, "resize array=%p, new_size=%d", array, new_size);
    array->size = new_size;
    if (new_size < array->count) {
        array->count = new_size;
    }
    array->data = ysw_heap_reallocate(array->data, array->size);
}

void ysw_array_pack(ysw_array_t *array)
{
    assert(array);
    ESP_LOGD(TAG, "pack array=%p", array);
    array->size = array->count;
    array->data = ysw_heap_reallocate(array->data, array->size);
}

int ysw_array_get_count(ysw_array_t *array)
{
    assert(array);
    ESP_LOGD(TAG, "get_count array=%p, count=%d", array, array->count);
    return array->count;
}

int ysw_array_get_size(ysw_array_t *array)
{
    assert(array);
    ESP_LOGD(TAG, "get_size array=%p, size=%d", array, array->size);
    return array->size;
}

void *ysw_array_get(ysw_array_t *array, int index)
{
    assert(array);
    ESP_LOGD(TAG, "get array=%p, index=%d, count=%d, size=%d", array, index, array->count, array->size);
    assert(array);
    assert(index < array->count);
    return array->data[index];
}

void ysw_array_set(ysw_array_t *array, int index, void *value)
{
    assert(array);
    assert(index < array->count);
    ESP_LOGD(TAG, "set array=%p, index=%d, value=%p, count=%d, size=%d", array, index, value, array->count, array->size);
    array->data[index] = value;
}

int ysw_array_get_free_space(ysw_array_t *array)
{
    assert(array);
    ESP_LOGD(TAG, "get_free_space array=%p, free_space=%d", array, array->size - array->count);
    return array->size - array->count;
}

void ysw_array_free_node(void *p)
{
    if (p) {
        ysw_heap_free(p);
    }
}

void ysw_array_clear(ysw_array_t *array, ysw_on_array_clear_t on_clear)
{
    for (size_t i = 0; i < array->count; i++) {
        on_clear(array->data[i]);
    }
    array->count = 0;
}

void ysw_array_free(ysw_array_t *array)
{
    assert(array);
    ESP_LOGD(TAG, "free array=%p, count=%d, size=%d", array, array->count, array->size);
    ysw_heap_free(array->data);
    ysw_heap_free(array);
}

