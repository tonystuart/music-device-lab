// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_task.h"
#include "esp_log.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"

#define TAG "YSW_TASK_STUB"

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
