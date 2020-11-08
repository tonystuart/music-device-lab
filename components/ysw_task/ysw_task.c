// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_task.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "assert.h"
#include "stdlib.h"

#define TAG "YSW_TASK"

typedef struct {
    ysw_bus_h bus;
    QueueHandle_t queue;
    ysw_task_event_handler_t event_handler;
    ysw_task_initializer initializer;
    void *caller_context;
    uint32_t wait_millis;
} context_t;

static void ysw_task_event_handler(void *parameter)
{
    context_t *context = parameter;
    if (context->initializer) {
        context->initializer(context->caller_context);
    }
    for (;;) {
        ysw_event_t item;
        ysw_event_t *event = NULL;
        TickType_t wait_ticks = ysw_millis_to_rtos_ticks(context->wait_millis);
        BaseType_t is_message = xQueueReceive(context->queue, &item, wait_ticks);
        if (is_message) {
            event = &item;
        } 
        context->event_handler(context->caller_context, event);
    }
}

const ysw_task_config_t ysw_task_default_config = {
    .name = TAG,
    .stack_size = YSW_TASK_DEFAULT_STACK_SIZE,
    .priority = YSW_TASK_DEFAULT_PRIORITY,
    .queue_size = YSW_TASK_DEFAULT_QUEUE_SIZE,
    .item_size = sizeof(ysw_event_t),
    .wait_millis = -1, // portDELAY_MAX,
};

ysw_task_h ysw_task_create(ysw_task_config_t *config)
{
    assert(config->stack_size);

    context_t *context = ysw_heap_allocate(sizeof(context_t));

    if (config->task) {
        *config->task = context;
    }

    context->bus = config->bus;
    context->wait_millis = config->wait_millis;
    context->initializer = config->initializer;

    TaskFunction_t function = NULL;
    void *parameter = NULL;

    if (config->event_handler) {
        assert(!config->function);
        function = ysw_task_event_handler;
        parameter = context;
        context->event_handler = config->event_handler;
        context->caller_context = config->caller_context;
    } else {
        assert(config->function);
        function = config->function;
        parameter = config->caller_context;
    }

    if (config->queue || config->bus) {
        assert(config->queue_size);
        assert(config->item_size);
        context->queue = xQueueCreate(config->queue_size, config->item_size);
        if (!context->queue) {
            ESP_LOGE(config->name, "ysw_task_create xQueueCreate failed");
            abort();
        }
        if (config->queue) {
            *config->queue = context->queue;
        }
    }

    BaseType_t rc = xTaskCreate(
            function,
            config->name,
            config->stack_size,
            parameter,
            config->priority,
            NULL);

    if (rc != pdPASS) {
        ESP_LOGE(config->name, "ysw_task_create xTaskCreate failed (%d)", rc);
        abort();
    }

    return context;
}

void ysw_task_subscribe(ysw_task_h task, ysw_origin_t origin)
{
    context_t *context = task;

    assert(context);
    assert(context->bus);
    assert(context->queue);

    ysw_bus_subscribe(context->bus, origin, context->queue);
}

void ysw_task_set_wait_millis(ysw_task_h task, uint32_t wait_millis)
{
    context_t *context = task;

    assert(context);

    context->wait_millis = wait_millis;
}

