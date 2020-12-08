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

typedef enum {
    YSW_ARRAY_MATCH_EXACT = 0x0000,
    YSW_ARRAY_MATCH_FLOOR = 0x0001,
    YSW_ARRAY_MATCH_CEIL = 0x0002,
    YSW_ARRAY_MATCH_LOWEST_INDEX = 0x0004,
    YSW_ARRAY_MATCH_HIGHEST_INDEX = 0x0008,
} ysw_array_match_t;

typedef struct {
    void **data;
    uint32_t count;
    uint32_t size;
} ysw_array_t;

typedef void (*ysw_array_clear_cb_t)(void *p);
typedef void (*ysw_array_visit_cb_t)(void *p);
typedef int (*ysw_array_comparator)(const void *, const void *);

ysw_array_t *ysw_array_create(uint32_t initial_size);
uint32_t ysw_array_push(ysw_array_t *array, void *data);
void *ysw_array_pop(ysw_array_t *array);
void *ysw_array_load(uint32_t count, ...);
void ysw_array_truncate(ysw_array_t *array, uint32_t new_count);
void ysw_array_resize(ysw_array_t *array, uint32_t new_size);
void ysw_array_pack(ysw_array_t *array);
uint32_t ysw_array_get_count(ysw_array_t *array);
uint32_t ysw_array_get_size(ysw_array_t *array);
void *ysw_array_get_top(ysw_array_t *array);
void *ysw_array_get(ysw_array_t *array, uint32_t index);
void ysw_array_set(ysw_array_t *array, uint32_t index, void *value);
void ysw_array_insert(ysw_array_t *array, uint32_t index, void *value);
void *ysw_array_remove(ysw_array_t *array, uint32_t index);
void ysw_array_swap(ysw_array_t *array, uint32_t i, uint32_t j);
void ysw_array_move(ysw_array_t *array, uint32_t from, uint32_t to);
int32_t ysw_array_find(ysw_array_t *array, void *value);
uint32_t ysw_array_get_free_space(ysw_array_t *array);
void ysw_array_sort(ysw_array_t *array, ysw_array_comparator comparator);
int32_t ysw_array_search(ysw_array_t *array, void *needle, ysw_array_comparator comparator, ysw_array_match_t match_type);
void ysw_array_visit(ysw_array_t *array, ysw_array_clear_cb_t clear_cb);
void ysw_array_clear(ysw_array_t *array, ysw_array_clear_cb_t clear_cb);
void ysw_array_free(ysw_array_t *array);
void ysw_array_free_node(void *p);
void ysw_array_free_all(ysw_array_t *array);

static inline void *ysw_array_get_fast(ysw_array_t *a, uint32_t i) {return a->data[i];}
static inline uint32_t ysw_array_get_count_fast(ysw_array_t *a) {return a->count;}
