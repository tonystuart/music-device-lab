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

ysw_array_t *ysw_array_create(uint32_t initial_size)
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

uint32_t ysw_array_push(ysw_array_t *array, void *value)
{
    assert(array);
    ESP_LOGD(TAG, "push array=%p, value=%p, count=%d, size=%d", array, value, array->count, array->size);
    uint32_t index = array->count++;
    if (array->count > array->size) {
        array->size = array->size ? array->size * 2 : 4;
        ESP_LOGD(TAG, "push resizing to size=%d", array->size);
        array->data = ysw_heap_reallocate(array->data, sizeof(void*) * array->size);
    }
    array->data[index] = value;
    return index;
}

void *ysw_array_pop(ysw_array_t *array)
{
    assert(array);
    assert(array->count);
    ESP_LOGD(TAG, "pop array=%p, count=%d, size=%d", array, array->count, array->size);
    uint32_t index = --array->count;
    return array->data[index];
}

void ysw_array_truncate(ysw_array_t *array, uint32_t new_count)
{
    assert(array);
    assert(new_count <= array->count);
    array->count = new_count;
}

void ysw_array_resize(ysw_array_t *array, uint32_t new_size)
{
    assert(array);
    ESP_LOGD(TAG, "resize array=%p, new_size=%d", array, new_size);
    array->size = new_size;
    if (new_size < array->count) {
        array->count = new_size;
    }
    array->data = ysw_heap_reallocate(array->data, sizeof(void*) * array->size);
}

void ysw_array_pack(ysw_array_t *array)
{
    assert(array);
    ESP_LOGD(TAG, "pack array=%p", array);
    array->size = array->count;
    array->data = ysw_heap_reallocate(array->data, sizeof(void*) * array->size);
}

uint32_t ysw_array_get_count(ysw_array_t *array)
{
    assert(array);
    ESP_LOGD(TAG, "get_count array=%p, count=%d", array, array->count);
    return array->count;
}

uint32_t ysw_array_get_size(ysw_array_t *array)
{
    assert(array);
    ESP_LOGD(TAG, "get_size array=%p, size=%d", array, array->size);
    return array->size;
}

void *ysw_array_get(ysw_array_t *array, uint32_t index)
{
    assert(array);
    ESP_LOGD(TAG, "get array=%p, index=%d, count=%d, size=%d", array, index, array->count, array->size);
    assert(array);
    assert(index < array->count);
    return array->data[index];
}

void ysw_array_set(ysw_array_t *array, uint32_t index, void *value)
{
    assert(array);
    assert(index < array->count);
    ESP_LOGD(TAG, "set array=%p, index=%d, value=%p, count=%d, size=%d", array, index, value, array->count, array->size);
    array->data[index] = value;
}

void ysw_array_insert(ysw_array_t *array, uint32_t index, void *value)
{
    assert(array);
    ESP_LOGD(TAG, "insert array=%p, index=%d, value=%p, count=%d, size=%d", array, index, value, array->count, array->size);
    array->count++;
    assert(index < array->count);
    if (array->count > array->size) {
        array->size = array->size ? array->size * 2 : 4;
        ESP_LOGD(TAG, "insert resizing to size=%d", array->size);
        array->data = ysw_heap_reallocate(array->data, sizeof(void*) * array->size);
    }
    for (uint32_t i = array->count - 1, j = array->count - 2; (int32_t)j >= (int32_t)index; i--, j--) {
        array->data[i] = array->data[j];
    }
    array->data[index] = value;
}

void ysw_array_remove(ysw_array_t *array, uint32_t index)
{
    assert(array);
    assert(index < array->count);
    ESP_LOGD(TAG, "remove array=%p, index=%d, count=%d, size=%d", array, index, array->count, array->size);
    array->count++;
    for (uint32_t i = index, j = index + 1; j < array->count; i++, j++) {
        array->data[i] = array->data[j];
    }
    array->count--;
}

int32_t ysw_array_find(ysw_array_t *array, void *value)
{
    for (uint32_t i = 0; i < array->count; i++) {
        if (array->data[i] == value) {
            return i;
        }
    }
    return -1;
}

uint32_t ysw_array_get_free_space(ysw_array_t *array)
{
    assert(array);
    ESP_LOGD(TAG, "get_free_space array=%p, free_space=%d", array, array->size - array->count);
    return array->size - array->count;
}

void ysw_array_sort(ysw_array_t *array,  int (*comparator)(const void *, const void *))
{
    qsort(array->data, array->count, sizeof(void *), comparator);
}

void ysw_array_free_node(void *p)
{
    if (p) {
        ysw_heap_free(p);
    }
}

void ysw_array_clear(ysw_array_t *array, ysw_on_array_clear_t on_clear)
{
    for (uint32_t i = 0; i < array->count; i++) {
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

