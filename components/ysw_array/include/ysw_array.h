// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "stddef.h"
#include "stdint.h"

typedef struct {
    void **data;
    uint32_t count;
    uint32_t size;
} ysw_array_t;

typedef void (*ysw_on_array_clear_t)(void *p);

ysw_array_t *ysw_array_create(uint32_t initial_size);
uint32_t ysw_array_push(ysw_array_t *array, void *data);
void *ysw_array_pop(ysw_array_t *array);
void ysw_array_truncate(ysw_array_t *array, uint32_t new_count);
void ysw_array_resize(ysw_array_t *array, uint32_t new_size);
void ysw_array_pack(ysw_array_t *array);
uint32_t ysw_array_get_count(ysw_array_t *array);
uint32_t ysw_array_get_size(ysw_array_t *array);
void *ysw_array_get(ysw_array_t *array, uint32_t index);
void ysw_array_set(ysw_array_t *array, uint32_t index, void *value);
void ysw_array_insert(ysw_array_t *array, uint32_t index, void *value);
void ysw_array_remove(ysw_array_t *array, uint32_t index);
void ysw_array_swap(ysw_array_t *array, uint32_t i, uint32_t j);
void ysw_array_move(ysw_array_t *array, uint32_t from, uint32_t to);
int32_t ysw_array_find(ysw_array_t *array, void *value);
uint32_t ysw_array_get_free_space(ysw_array_t *array);
void ysw_array_sort(ysw_array_t *array,  int (*comparator)(const void *, const void *));
void ysw_array_free_node(void *p);
void ysw_array_clear(ysw_array_t *array, ysw_on_array_clear_t on_clear);
void ysw_array_free(ysw_array_t *array);
