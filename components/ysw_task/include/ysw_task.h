// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "stdint.h"

#define YSW_TASK_DEFAULT_PRIORITY (tskIDLE_PRIORITY + 1)
#define YSW_TASK_DEFAULT_STACK_SIZE 4096 // size in 32 bit words
#define YSW_TASK_DEFAULT_QUEUE_SIZE 16

typedef void (*ysw_task_event_handler_t)(void *context, ysw_event_t *event);

typedef void (*ysw_task_initializer)(void *context);

typedef struct {
    ysw_bus_t *bus;
    QueueHandle_t queue;
    ysw_task_event_handler_t event_handler;
    ysw_task_initializer initializer;
    void *context;
    uint32_t wait_millis;
} ysw_task_t;

// 1. Specify queue and/or bus if you want a queue allocated
// 2. Specify queue_size and item_size if you want a queue allocated
// 3. Specify event_handler if you want an event handler wrapper
// 4. Specify function if you do not want an event handler wrapper
// 5. Specify either event_handler or function but not both
// 6. Specify context if you want to pass context to event_handler or function
// 7. Specify task and/or queue if you want the created task or queue handle returned
// 8. Specify non-transient (i.e. not stack) address for task or queue
// 9. Specify use_current_task=true to run in current task (e.g. main task)

typedef struct {
    const char *name;
    ysw_bus_t *bus;
    ysw_task_t **task;
    TaskFunction_t function;
    ysw_task_event_handler_t event_handler;
    ysw_task_initializer initializer;
    void *context;
    uint16_t stack_size;
    int8_t priority;
    bool use_current_task;
    QueueHandle_t *queue;
    UBaseType_t queue_size;
    UBaseType_t item_size;
    uint32_t wait_millis;
} ysw_task_config_t;

extern const ysw_task_config_t ysw_task_default_config;

ysw_task_t *ysw_task_create(ysw_task_config_t *config);

void ysw_task_subscribe(ysw_task_t *task, ysw_origin_t origin);

void ysw_task_set_wait_millis(ysw_task_t *task, uint32_t wait_millis);

