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
} context_t;

ysw_pool_h ysw_pool_create(uint32_t initial_size)
{
    assert(initial_size > 0);

    context_t *context = ysw_heap_allocate(sizeof(context_t));
    context->array = ysw_array_create(initial_size);
    return context;
}

void ysw_pool_add(ysw_pool_h pool, void *item)
{
    assert(pool);
    assert(item);

    context_t *context = (context_t *)pool;
    ysw_array_push(context->array, item);
}

void* ysw_pool_visit_items(ysw_pool_h pool, ysw_pool_visitor_t visitor, void *caller_context)
{
    assert(pool);
    assert(visitor);

    void *item = NULL;
    bool done = false;
    uint32_t index = 0;
    context_t *context = (context_t *)pool;
    uint32_t count = ysw_array_get_count(context->array);
    while (index < count && !done) {
        item = ysw_array_get(context->array, index);
        ysw_pool_action_t action = visitor(caller_context, index, count, item);
        if (action & YSW_POOL_ACTION_FREE) {
            uint32_t top = count - 1;
            if (index < top) {
                void *last_item = ysw_array_get(context->array, top);
                ysw_array_set(context->array, index, last_item);
            }
            count--;
            ysw_array_truncate(context->array, count);
        }
        if (action & YSW_POOL_ACTION_STOP) {
            done = true;
        } else {
            index++;
        }
    }
    return item;
}

void ysw_pool_free(ysw_pool_h pool)
{
    assert(pool);

    context_t *context = (context_t *)pool;
    ysw_array_free(context->array);
    ysw_heap_free(context);
}
