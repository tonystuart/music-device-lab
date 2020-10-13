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

#define TAG "YSW_TASK"

#ifdef IDF_VER

void ysw_task_create(ysw_task_config_t *config)
{
    ESP_LOGD(TAG, "ysw_task_create queue_size=%d, item_size=%d, stack_size=%d, priority=%d, is_relative=%d", config->queue_size, config->item_size, config->stack_size, config->priority, config->is_relative_priority);
    if (config->queue) {
        *config->queue = xQueueCreate(config->queue_size, config->item_size);
        if (!*config->queue) {
            ESP_LOGE(config->name, "ysw_task_create xQueueCreate failed");
            abort();
        }
    }
    UBaseType_t new_task_priority;
    if (config->is_relative_priority) {
        UBaseType_t current_task_priority = uxTaskPriorityGet(NULL);
        new_task_priority = current_task_priority + config->priority;
    } else {
        new_task_priority = config->priority;
    }
    BaseType_t rc = xTaskCreate(config->function, config->name, config->stack_size, config->parameters, new_task_priority, config->task);
    if (rc != pdPASS) {
        ESP_LOGE(config->name, "ysw_task_create xTaskCreate failed (%d)", rc);
        abort();
    }
}

#else

#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"

void ysw_task_create(ysw_task_config_t *config)
{
    if (config->queue) {
        *config->queue = xQueueCreate(config->queue_size, config->item_size);
        if (!*config->queue) {
            ESP_LOGE(config->name, "xQueueCreate failed");
            abort();
        }
    }
    pthread_t tid;
    int rc = pthread_create(&tid, NULL, config->function, config->parameters);
    if (rc == -1) {
        ESP_LOGE(TAG, "pthread_create failed");
        abort();
    }
}

#endif

void ysw_task_create_standard(char *name, TaskFunction_t function, QueueHandle_t *queue, UBaseType_t item_size)
{
    ysw_task_config_t config = {
        .name = name,
        .function = function,
        .queue = queue,
        .queue_size = YSW_TASK_MEDIUM_QUEUE,
        .item_size = item_size,
        .stack_size = YSW_TASK_MEDIUM_STACK,
        .priority = YSW_TASK_DEFAULT_PRIORITY,
    };
    ysw_task_create(&config);
}

typedef struct {
    QueueHandle_t input_queue;
    ysw_task_event_handler event_handler;
    void *caller_context;
} context_t;

static void task_handler(void *parameter)
{
    context_t *context = parameter;
    for (;;) {
        ysw_event_t event;
        BaseType_t is_message = xQueueReceive(context->input_queue, &event, portMAX_DELAY);
        if (is_message) {
            context->event_handler(context->caller_context, &event);
        }
    }
}

QueueHandle_t ysw_task_create_event_task(ysw_task_event_handler event_handler, void *caller_context)
{
    context_t *context = ysw_heap_allocate(sizeof(context_t));

    context->event_handler = event_handler;
    context->caller_context = caller_context;

    ysw_task_config_t config = {
        .name = TAG,
        .function = task_handler,
        .parameters = context,
        .queue = &context->input_queue,
        .queue_size = 16,
        .item_size = sizeof(ysw_event_t),
        .stack_size = YSW_TASK_MEDIUM_STACK,
        .priority = YSW_TASK_DEFAULT_PRIORITY,
    };

    ysw_task_create(&config);

    return context->input_queue;
}
