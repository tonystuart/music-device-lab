// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_bus.h"

#include "ysw_heap.h"
#include "freertos/task.h"
#include "stdlib.h"

typedef struct {
    void *context;
    ysw_bus_cb_t cb;
} ysw_bus_listener_t;

static uint32_t allocate_handle(ysw_bus_t *bus)
{
    uint32_t count = ysw_array_get_count(bus->listeners);
    for (uint32_t i = 0; i < count; i++) {
        ysw_bus_listener_t *listener = ysw_array_get(bus->listeners, i);
        if (!listener) {
            return i;
        }
    }
    uint32_t handle = ysw_array_push(bus->listeners, NULL);
    return handle;
}

ysw_bus_t* ysw_bus_create()
{
    ysw_bus_t *bus = ysw_heap_allocate(sizeof(ysw_bus_t));
    return bus;
}

uint32_t ysw_bus_subscribe(ysw_bus_t *bus, void *context, void *cb)
{
    ysw_bus_listener_t *listener = ysw_heap_allocate(sizeof(ysw_bus_listener_t));
    listener->context = context;
    listener->cb = cb;
    uint32_t index = allocate_handle(bus);
    ysw_array_set(bus->listeners, index, listener);
    return index;
}

uint32_t ysw_bus_publish(ysw_bus_t *bus, uint32_t msg, void *details)
{
    uint32_t publish_count = 0;
    uint32_t listener_count = ysw_array_get_count(bus->listeners);
    char *sender = pcTaskGetTaskName((TaskHandle_t)NULL);
    for (uint32_t i = 0; i < listener_count; i++) {
        ysw_bus_listener_t *listener = ysw_array_get(bus->listeners, i);
        if (listener) {
            listener->cb(listener->context, msg, details, sender);
            publish_count++;
        }
    }
    return publish_count;
}

void ysw_bus_unsubscribe(ysw_bus_t *bus, uint32_t handle)
{
    ysw_bus_listener_t *listener = ysw_array_get(bus->listeners, handle);
    ysw_heap_free(listener);
    ysw_array_set(bus->listeners, handle, NULL);
}

void ysw_bus_free(ysw_bus_t *bus)
{
    uint32_t count = ysw_array_get_count(bus->listeners);
    for (uint32_t i = 0; i < count; i++) {
        ysw_bus_listener_t *listener = ysw_array_get(bus->listeners, i);
        if (listener) {
            ysw_heap_free(listener);
        }
    }
    ysw_heap_free(bus);
}
