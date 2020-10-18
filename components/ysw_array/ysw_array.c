// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_array.h"

#include "ysw_heap.h"
#include "esp_log.h"
#include "assert.h"
#include "stdarg.h"
#include "stdlib.h"

#define TAG "YSW_ARRAY"

ysw_array_t *ysw_array_create(uint32_t initial_size)
{
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
    uint32_t index = array->count++;
    if (array->count > array->size) {
        array->size = array->size ? array->size * 2 : 4;
        array->data = ysw_heap_reallocate(array->data, sizeof(void*) * array->size);
    }
    array->data[index] = value;
    return index;
}

void *ysw_array_pop(ysw_array_t *array)
{
    assert(array);
    assert(array->count);
    uint32_t index = --array->count;
    return array->data[index];
}

void *ysw_array_load(uint32_t count, ...)
{
    ysw_array_t *array = ysw_array_create(count);
    va_list arguments;
    va_start(arguments, count);
    for (uint32_t i = 0; i < count; i++) {
        uintptr_t value = va_arg(arguments, uintptr_t);
        ESP_LOGD(TAG, "ysw_array_load count=%d, i=%d, value=%d", count, i, (int)value);
        ysw_array_set(array, i, (void *)value);
    }
    va_end(arguments);
    return array;
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
    array->size = new_size;
    if (new_size < array->count) {
        array->count = new_size;
    }
    array->data = ysw_heap_reallocate(array->data, sizeof(void*) * array->size);
}

void ysw_array_pack(ysw_array_t *array)
{
    assert(array);
    array->size = array->count;
    array->data = ysw_heap_reallocate(array->data, sizeof(void*) * array->size);
}

uint32_t ysw_array_get_count(ysw_array_t *array)
{
    assert(array);
    return array->count;
}

uint32_t ysw_array_get_size(ysw_array_t *array)
{
    assert(array);
    return array->size;
}

void *ysw_array_get(ysw_array_t *array, uint32_t index)
{
    assert(array);
    assert(index < array->count);
    return array->data[index];
}

void ysw_array_set(ysw_array_t *array, uint32_t index, void *value)
{
    assert(array);
    assert(index < array->size);
    if (array->count <= index) {
        array->count = index + 1;
    }
    array->data[index] = value;
}

void ysw_array_insert(ysw_array_t *array, uint32_t index, void *value)
{
    assert(array);
    array->count++;
    assert(index < array->count);
    if (array->count > array->size) {
        array->size = array->size ? array->size * 2 : 4;
        array->data = ysw_heap_reallocate(array->data, sizeof(void*) * array->size);
    }
    for (uint32_t i = array->count - 1, j = array->count - 2; (int32_t)j >= (int32_t)index; i--, j--) {
        array->data[i] = array->data[j];
    }
    array->data[index] = value;
}

void *ysw_array_remove(ysw_array_t *array, uint32_t index)
{
    assert(array);
    assert(index < array->count);
    void *item = array->data[index];
    for (uint32_t i = index, j = index + 1; j < array->count; i++, j++) {
        array->data[i] = array->data[j];
    }
    array->count--;
    return item;
}

void ysw_array_swap(ysw_array_t *array, uint32_t i, uint32_t j)
{
    assert(array);
    assert(i < array->count);
    assert(j < array->count);
    void *p = array->data[i];
    array->data[i] = array->data[j];
    array->data[j] = p;
}

void ysw_array_move(ysw_array_t *array, uint32_t from, uint32_t to)
{
    assert(array);
    assert(from < array->count);
    assert(to < array->count);
    if (from < to) {
        // moving right, everything shifts left
        void *temp = array->data[from];
        for (uint32_t i = from; i < to; i++) {
            array->data[i] = array->data[i+1];
        }
        array->data[to] = temp;
    } else if (to < from) {
        // moving left, everything shifts right
        void *temp = array->data[from];
        for (uint32_t i = from; i > to; i--) {
            array->data[i] = array->data[i-1];
        }
        array->data[to] = temp;
    }
}

int32_t ysw_array_find(ysw_array_t *array, void *value)
{
    assert(array);
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
    return array->size - array->count;
}

void ysw_array_sort(ysw_array_t *array,  int (*comparator)(const void *, const void *))
{
    assert(array);
    assert(comparator);
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
    assert(array);
    assert(on_clear);
    for (uint32_t i = 0; i < array->count; i++) {
        on_clear(array->data[i]);
    }
    array->count = 0;
}

void ysw_array_free(ysw_array_t *array)
{
    assert(array);
    ysw_heap_free(array->data);
    ysw_heap_free(array);
}

void ysw_array_free_all(ysw_array_t *array)
{
    assert(array);
    ysw_array_clear(array, ysw_array_free_node);
    ysw_array_free(array);
}

