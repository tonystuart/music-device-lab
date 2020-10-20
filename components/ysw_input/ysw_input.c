// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.


#include "ysw_input.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_task.h"
#include "esp_log.h"

#define TAG "INPUT"

typedef struct {
    ysw_bus_h bus;
} context_t;

static const uint8_t key_map[] = {
    /* 1 */   13, 15,     18, 20, 22,     24, 25, 26, 27,
    /* 2 */ 12, 14, 16, 17, 19, 21, 23,   28, 29, 30, 31,
    /* 3 */    1,  3,      6,  8, 10,     32, 33, 34, 35,
    /* 4 */  0,  2,  4,  5,  7,  9, 11,   36, 37, 38, 39,
};

#define KEY_MAP_SZ (sizeof(key_map) / sizeof(key_map[0]))

static void on_key_down(context_t *context, ysw_event_key_down_t *event)
{
    assert(event->key < KEY_MAP_SZ);
    uint8_t value = key_map[event->key];
    if (value < 24) {
        ysw_event_fire_note_on(context->bus, 0, 60 + value, 80);
    }
}

static void on_key_up(context_t *context, ysw_event_key_up_t *event)
{
    assert(event->key < KEY_MAP_SZ);
    uint8_t value = key_map[event->key];
    if (value < 24) {
        ysw_event_fire_note_off(context->bus, 0, 60 + value);
    }
}

static void process_event(void *caller_context, ysw_event_t *event)
{
    context_t *context = caller_context;
    switch (event->header.type) {
        case YSW_EVENT_KEY_DOWN:
            on_key_down(context, &event->key_down);
            break;
        case YSW_EVENT_KEY_UP:
            on_key_up(context, &event->key_up);
            break;
        default:
            break;
    }
}

void ysw_input_create_task(ysw_bus_h bus)
{
    context_t *context = ysw_heap_allocate(sizeof(context_t));

    context->bus = bus;

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.event_handler = process_event;
    config.caller_context = context;

    ysw_task_h task = ysw_task_create(&config);
    ysw_task_subscribe(task, YSW_ORIGIN_KEYBOARD);
}
