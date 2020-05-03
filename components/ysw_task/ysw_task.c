// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_task.h"

#define TAG "YSW_TASK"

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
