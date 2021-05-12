// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_mapper.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_task.h"
#include "esp_log.h"

#define TAG "YSW_MAPPER"

typedef struct {
    ysw_bus_t *bus;
    const ysw_mapper_item_t *map;
} ysw_mapper_t;

static void on_key_down(ysw_mapper_t *mapper, ysw_event_key_down_t *m)
{
    ysw_mapper_item_t item = mapper->map[m->scan_code];
    ESP_LOGD(TAG, "mapping scan_code=%d to item=%d", m->scan_code, item);
    if (item < 0) {
        ysw_event_notekey_down_t notekey_down = {
            .midi_note = -item,
            .time = m->time,
        };
        ysw_event_fire_notekey_down(mapper->bus, &notekey_down);
    } else if (item > 0) {
        ysw_event_softkey_down_t softkey_down = {
            .softkey = item,
            .time = m->time,
        };
        ysw_event_fire_softkey_down(mapper->bus, &softkey_down);
    }
}

static void on_key_up(ysw_mapper_t *mapper, ysw_event_key_up_t *m)
{
    ysw_mapper_item_t item = mapper->map[m->scan_code];
    if (item < 0) {
        ysw_event_notekey_up_t notekey_up = {
            .midi_note = -item,
            .time = m->time,
            .duration = m->duration,
        };
        ysw_event_fire_notekey_up(mapper->bus, &notekey_up);
    } else if (item > 0) {
        ysw_event_softkey_up_t softkey_up = {
            .softkey = item,
            .time = m->time,
            .duration = m->duration,
            .repeat_count = m->repeat_count,
        };
        ysw_event_fire_softkey_up(mapper->bus, &softkey_up);
    }
}

static void on_key_pressed(ysw_mapper_t *mapper, ysw_event_key_pressed_t *m)
{
    ysw_mapper_item_t item = mapper->map[m->scan_code];
    if (item < 0) {
        // notekeys don't do pressed
    } else if (item > 0) {
        ysw_event_softkey_pressed_t softkey_pressed = {
            .softkey = item,
            .time = m->time,
            .duration = m->duration,
            .repeat_count = m->repeat_count,
        };
        ysw_event_fire_softkey_pressed(mapper->bus, &softkey_pressed);
    }
}

static void process_event(void *context, ysw_event_t *event)
{
    ysw_mapper_t *mapper = context;
    switch (event->header.type) {
        case YSW_EVENT_KEY_DOWN:
            on_key_down(mapper, &event->key_down);
            break;
        case YSW_EVENT_KEY_UP:
            on_key_up(mapper, &event->key_up);
            break;
        case YSW_EVENT_KEY_PRESSED:
            on_key_pressed(mapper, &event->key_pressed);
            break;
        default:
            break;
    }
}

void ysw_mapper_create_task(ysw_bus_t *bus, const ysw_mapper_item_t *map)
{
    ysw_mapper_t *mapper = ysw_heap_allocate(sizeof(ysw_mapper_t));

    mapper->bus = bus;
    mapper->map = map;

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.event_handler = process_event;
    config.context = mapper;

    ysw_task_t *task = ysw_task_create(&config);
    ysw_task_subscribe(task, YSW_ORIGIN_KEYBOARD);
}

