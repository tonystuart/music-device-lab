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
#include "ysw_message.h"
#include "ysw_task.h"
#include "freertos/task.h"
#include "assert.h"
#include "stdlib.h"

#define TAG "YSW_BUS"

typedef enum {
    YSW_BUS_SUBSCRIBE,
    YSW_BUS_PUBLISH,
    YSW_BUS_UNSUBSCRIBE,
    YSW_BUS_FREE,
} ysw_bus_msg_type_t;

typedef struct {
    ysw_origin_t origin;
    QueueHandle_t queue;
} ysw_bus_subscribe_t;

typedef struct {
    ysw_origin_t origin;
    char message[]; // flexible array member
} ysw_bus_publish_t;

typedef struct {
    ysw_origin_t origin;
    QueueHandle_t queue;
} ysw_bus_unsubscribe_t;

typedef struct {
    ysw_bus_msg_type_t type;
    union {
        ysw_bus_subscribe_t subscribe_info;
        ysw_bus_publish_t publish_info;
        ysw_bus_unsubscribe_t unsubscribe_info;
    };
} ysw_bus_msg_t;

static ysw_pool_action_t publish(void *message, uint32_t index, uint32_t count, void *item)
{
    ysw_message_send(item, message);
    return YSW_POOL_ACTION_NOP;
}

static ysw_pool_action_t unsubscribe(void *queue, uint32_t index, uint32_t count, void *item)
{
    ysw_pool_action_t action = YSW_POOL_ACTION_NOP;
    if (queue == item) {
        action = (YSW_POOL_ACTION_FREE | YSW_POOL_ACTION_STOP);
    }
    return action;
}

static void process_subscribe(ysw_bus_t *bus, ysw_bus_subscribe_t *info)
{
    if (!bus->listeners[info->origin]) {
        bus->listeners[info->origin] = ysw_pool_create(bus->listeners_size);
    }
    ysw_pool_add(bus->listeners[info->origin], info->queue);
}

static void process_publish(ysw_bus_t *bus, ysw_bus_publish_t *info)
{
    if (bus->listeners[info->origin]) {
        ysw_pool_visit_items(bus->listeners[info->origin], publish, &info->message);
    }
}

static void process_unsubscribe(ysw_bus_t *bus, ysw_bus_unsubscribe_t *info)
{
    if (bus->listeners[info->origin]) {
        ysw_pool_visit_items(bus->listeners[info->origin], unsubscribe, &info->queue);
    }
}

static void process_free(ysw_bus_t *bus)
{
    for (uint32_t i = 0; i < bus->listeners_size; i++) {
        ysw_pool_free(bus->listeners[i]);
    }
    ysw_heap_free(bus);
    vTaskDelete(NULL);
}

static void process_message(ysw_bus_t *bus, ysw_bus_msg_t *message)
{
    switch (message->type) {
        case YSW_BUS_SUBSCRIBE:
            process_subscribe(bus, &message->subscribe_info);
            break;
        case YSW_BUS_PUBLISH:
            process_publish(bus, &message->publish_info);
            break;
        case YSW_BUS_UNSUBSCRIBE:
            process_unsubscribe(bus, &message->unsubscribe_info);
            break;
        case YSW_BUS_FREE:
            process_free(bus);
            break;
    }
}

static void task_handler(void *parameters)
{
    ysw_bus_t *bus = parameters;
    ysw_bus_msg_t *message = alloca(bus->bus_message_size);
    for (;;) {
        BaseType_t is_message = xQueueReceive(bus->queue, message, portMAX_DELAY);
        if (is_message) {
            process_message(bus, message);
        }
    }
}

ysw_bus_t *ysw_bus_create(uint16_t origins_size, uint16_t listeners_size, uint32_t queue_size, uint32_t message_size)
{
    uint32_t bus_size = sizeof(ysw_bus_t) + (origins_size * sizeof(ysw_pool_t *));
    ysw_bus_t *bus = ysw_heap_allocate(bus_size);

    bus->origins_size = origins_size;
    bus->listeners_size = listeners_size;
    bus->bus_message_size = sizeof(ysw_bus_msg_t) + message_size;

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.function = task_handler;
    config.context = bus;
    config.queue = &bus->queue;
    config.queue_size = queue_size;
    config.item_size = bus->bus_message_size;

    ysw_task_create(&config);

    return bus;
}

void ysw_bus_subscribe(ysw_bus_t *bus, ysw_origin_t origin, QueueHandle_t queue)
{
    assert(origin < bus->origins_size);

    ysw_bus_msg_t msg = {
        .type = YSW_BUS_SUBSCRIBE,
        .subscribe_info.origin = origin,
        .subscribe_info.queue = queue,
    };

    ysw_message_send(bus->queue, &msg);
}

void ysw_bus_publish(ysw_bus_t *bus, ysw_origin_t origin, void *message, uint32_t length)
{
    uint32_t message_length = sizeof(ysw_bus_msg_t) + length;

    assert(origin < bus->origins_size);
    assert(message_length <= bus->bus_message_size);

    ysw_bus_msg_t *msg = alloca(bus->bus_message_size);

    memset(msg, 0, bus->bus_message_size);
    memcpy(msg->publish_info.message, message, length);

    msg->type = YSW_BUS_PUBLISH;
    msg->publish_info.origin = origin;

    ysw_message_send(bus->queue, msg);
}

void ysw_bus_unsubscribe(ysw_bus_t *bus, ysw_origin_t origin, QueueHandle_t queue)
{
    assert(origin < bus->origins_size);

    ysw_bus_msg_t msg = {
        .type = YSW_BUS_UNSUBSCRIBE,
        .unsubscribe_info.origin = origin,
        .unsubscribe_info.queue = queue,
    };

    ysw_message_send(bus->queue, &msg);
}

void ysw_bus_free(ysw_bus_t *bus)
{
    ysw_bus_msg_t msg = {
        .type = YSW_BUS_FREE,
    };

    ysw_message_send(bus->queue, &msg);
}
