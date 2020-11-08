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
#include "ysw_pool.h"
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

typedef struct {
    QueueHandle_t queue;
    uint16_t origins_size;
    uint16_t listeners_size;
    uint16_t bus_message_size;
    ysw_pool_h listeners[]; // flexible array member
} context_t;

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

static void process_subscribe(context_t *context, ysw_bus_subscribe_t *info)
{
    if (!context->listeners[info->origin]) {
        context->listeners[info->origin] = ysw_pool_create(context->listeners_size);
    }
    ysw_pool_add(context->listeners[info->origin], info->queue);
}

static void process_publish(context_t *context, ysw_bus_publish_t *info)
{
    if (context->listeners[info->origin]) {
        ysw_pool_visit_items(context->listeners[info->origin], publish, &info->message);
    }
}

static void process_unsubscribe(context_t *context, ysw_bus_unsubscribe_t *info)
{
    if (context->listeners[info->origin]) {
        ysw_pool_visit_items(context->listeners[info->origin], unsubscribe, &info->queue);
    }
}

static void process_free(context_t *context)
{
    ysw_pool_free(context->listeners);
    ysw_heap_free(context);
    vTaskDelete(NULL);
}

static void process_message(context_t *context, ysw_bus_msg_t *message)
{
    switch (message->type) {
        case YSW_BUS_SUBSCRIBE:
            process_subscribe(context, &message->subscribe_info);
            break;
        case YSW_BUS_PUBLISH:
            process_publish(context, &message->publish_info);
            break;
        case YSW_BUS_UNSUBSCRIBE:
            process_unsubscribe(context, &message->unsubscribe_info);
            break;
        case YSW_BUS_FREE:
            process_free(context);
            break;
    }
}

static void task_handler(void *parameters)
{
    context_t *context = parameters;
    ysw_bus_msg_t *message = alloca(context->bus_message_size);
    for (;;) {
        BaseType_t is_message = xQueueReceive(context->queue, message, portMAX_DELAY);
        if (is_message) {
            process_message(context, message);
        }
    }
}

ysw_bus_h ysw_bus_create(uint16_t origins_size, uint16_t listeners_size, uint32_t queue_size, uint32_t message_size)
{
    uint32_t context_size = sizeof(context_t) + (origins_size * sizeof(ysw_pool_h));
    context_t *context = ysw_heap_allocate(context_size);

    context->origins_size = origins_size;
    context->listeners_size = listeners_size;
    context->bus_message_size = sizeof(ysw_bus_msg_t) + message_size;

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.function = task_handler;
    config.caller_context = context;
    config.queue = &context->queue;
    config.queue_size = queue_size;
    config.item_size = context->bus_message_size;

    ysw_task_create(&config);

    return context;
}

void ysw_bus_subscribe(ysw_bus_h bus, ysw_origin_t origin, QueueHandle_t queue)
{
    context_t *context = bus;
    assert(origin < context->origins_size);

    ysw_bus_msg_t msg = {
        .type = YSW_BUS_SUBSCRIBE,
        .subscribe_info.origin = origin,
        .subscribe_info.queue = queue,
    };

    ysw_message_send(context->queue, &msg);
}

void ysw_bus_publish(ysw_bus_h bus, ysw_origin_t origin, void *message, uint32_t length)
{
    context_t *context = bus;

    uint32_t message_length = sizeof(ysw_bus_msg_t) + length;

    assert(origin < context->origins_size);
    assert(message_length <= context->bus_message_size);

    ysw_bus_msg_t *msg = alloca(context->bus_message_size);

    memset(msg, 0, context->bus_message_size);
    memcpy(msg->publish_info.message, message, length);

    msg->type = YSW_BUS_PUBLISH;
    msg->publish_info.origin = origin;

    ysw_message_send(context->queue, msg);
}

void ysw_bus_unsubscribe(ysw_bus_h bus, ysw_origin_t origin, QueueHandle_t queue)
{
    context_t *context = bus;
    assert(origin < context->origins_size);

    ysw_bus_msg_t msg = {
        .type = YSW_BUS_UNSUBSCRIBE,
        .unsubscribe_info.origin = origin,
        .unsubscribe_info.queue = queue,
    };

    ysw_message_send(context->queue, &msg);
}

void ysw_bus_free(ysw_bus_h bus)
{
    context_t *context = bus;

    ysw_bus_msg_t msg = {
        .type = YSW_BUS_FREE,
    };

    ysw_message_send(context->queue, &msg);
}
