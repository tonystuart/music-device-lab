// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_array.h"
#include "stdbool.h"
#include "stdint.h"

typedef struct {
    ysw_array_t *array;
} ysw_pool_t;

typedef enum {
    YSW_POOL_ACTION_NOP = 0x00,
    YSW_POOL_ACTION_STOP = 0x01,
    YSW_POOL_ACTION_FREE = 0x02,
} ysw_pool_action_t;

typedef ysw_pool_action_t (*ysw_pool_visitor_t)(void *context, uint32_t index, uint32_t count, void *item);

ysw_pool_t *ysw_pool_create(uint32_t initial_size);
void ysw_pool_add(ysw_pool_t *pool, void *item);
void *ysw_pool_visit_items(ysw_pool_t *pool, ysw_pool_visitor_t visitor, void *context);
void ysw_pool_free(ysw_pool_t *pool);


