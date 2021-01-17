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

static void ensure_capacity(ysw_array_t *array, uint32_t required_size)
{
    if (required_size > array->size) {
        array->size = array->size ? array->size * 2 : 4;
        array->data = ysw_heap_reallocate(array->data, sizeof(void*) * array->size);
    }
}

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
    ensure_capacity(array, array->count);
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

void ysw_array_set_count(ysw_array_t *array, uint32_t new_count)
{
    assert(array);
    ensure_capacity(array, new_count);
    array->count = new_count;
}

void ysw_array_set_size(ysw_array_t *array, uint32_t new_size)
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

void *ysw_array_get_top(ysw_array_t *array)
{
    assert(array);
    assert(array->count);
    return array->data[array->count - 1];
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

void ysw_array_sort(ysw_array_t *array, ysw_array_comparator comparator)
{
    assert(array);
    assert(comparator);
    qsort(array->data, array->count, sizeof(void *), comparator);
}

int32_t ysw_array_search(ysw_array_t *array, void *needle, ysw_array_comparator comparator, ysw_array_match_t match_type)
{
    assert(array);
    assert(comparator);

    int16_t found = -1;
    uint16_t bottom = 0;
    int16_t middle = -1;
    uint16_t top = array->count;

    while (found == -1 && bottom != top) {
        middle = bottom + ((top - bottom) / 2);
        int relationship = comparator(&needle, &array->data[middle]);
        if (relationship < 0) {
            top = middle;
        } else if (relationship > 0) {
            bottom = middle + 1;
        } else if ((match_type & YSW_ARRAY_MATCH_LOWEST_INDEX) &&
                (middle > 0 && comparator(&array->data[middle-1], &array->data[middle]) == 0)) {
            top = middle;
        } else if ((match_type & YSW_ARRAY_MATCH_HIGHEST_INDEX) &&
                (middle + 1 < array->count && comparator(&array->data[middle+1], &array->data[middle]) == 0)) {
            bottom = middle + 1;
        } else {
            found = middle;
        }
    }

    if (!found) {
        if ((match_type & YSW_ARRAY_MATCH_FLOOR) &&
                (middle > 0 && comparator(&array->data[middle], &needle) < 0)) {
            found = middle - 1;
        } else if ((match_type & YSW_ARRAY_MATCH_CEIL) &&
                (middle + 1 < array->count && comparator(&needle, &array->data[middle+1]) < 0)) {
            found = middle + 1;
        }
    }

    return found;
}

void ysw_array_visit(ysw_array_t *array, ysw_array_visit_cb_t visit_cb)
{
    assert(array);
    assert(visit_cb);
    for (uint32_t i = 0; i < array->count; i++) {
        visit_cb(array->data[i]);
    }
}

void ysw_array_free(ysw_array_t *array)
{
    assert(array);
    ysw_heap_free(array->data);
    ysw_heap_free(array);
}

void ysw_array_free_node(void *p)
{
    if (p) {
        ysw_heap_free(p);
    }
}

void ysw_array_set_free_cb(ysw_array_t *array, ysw_array_free_cb_t free_cb)
{
    array->free_cb = free_cb;
}

void ysw_array_free_elements(ysw_array_t *array)
{
    ysw_array_free_cb_t free_cb = NULL;
    if (array->free_cb) {
        free_cb = array->free_cb;
    } else {
        free_cb = ysw_array_free_node;
    }
    ysw_array_visit(array, free_cb);
    array->count = 0;
}

void ysw_array_free_all(ysw_array_t *array)
{
    assert(array);
    ysw_array_free_elements(array);
    ysw_array_free(array);
}
