// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_pool.h"

#include "ysw_array.h"
#include "ysw_heap.h"
#include "assert.h"
#include "stdlib.h"

typedef struct {
    ysw_array_t *array;
} ysw_pool_t;

ysw_pool_handle_t ysw_pool_create(uint32_t initial_size)
{
    assert(initial_size > 0);
    ysw_pool_t *pool = ysw_heap_allocate(sizeof(ysw_pool_t));
    pool->array = ysw_array_create(initial_size);
    return pool;
}

void ysw_pool_add(ysw_pool_handle_t handle, void *item)
{
    assert(handle);
    assert(item);
    ysw_pool_t *pool = (ysw_pool_t *)handle;
    ysw_array_push(pool->array, item);
}

void* ysw_pool_visit_items(ysw_pool_handle_t handle, ysw_pool_visitor_t visitor, void *context)
{
    assert(handle);
    assert(visitor);
    void *item = NULL;
    bool done = false;
    uint32_t index = 0;
    ysw_pool_t *pool = (ysw_pool_t *)handle;
    uint32_t count = ysw_array_get_count(pool->array);
    while (index < count && !done) {
        item = ysw_array_get(pool->array, index);
        ysw_pool_action_t action = visitor(context, index, count, item);
        if (action & YSW_POOL_ACTION_FREE) {
            uint32_t top = count - 1;
            if (index < top) {
                void *last_item = ysw_array_get(pool->array, top);
                ysw_array_set(pool->array, index, last_item);
            }
            count--;
            ysw_array_truncate(pool->array, count);
        } else {
            index++;
        }
    }
    return item;
}

void ysw_pool_free(ysw_pool_handle_t handle)
{
    assert(handle);
    ysw_pool_t *pool = (ysw_pool_t *)handle;
    ysw_array_free(pool->array);
    ysw_heap_free(pool);
}
