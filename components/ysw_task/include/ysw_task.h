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

// FreeRTOS stack sizes are in 32 bit words

#define YSW_TASK_TINY_STACK 1024
#define YSW_TASK_SMALL_STACK 2048
#define YSW_TASK_MEDIUM_STACK 4096
#define YSW_TASK_LARGE_STACK 8192
#define YSW_TASK_HUGE_STACK 16384

#define YSW_TASK_SMALL_QUEUE 2
#define YSW_TASK_MEDIUM_QUEUE 4
#define YSW_TASK_LARGE_QUEUE 8

typedef struct {
    char *name;
    TaskHandle_t *task;
    TaskFunction_t function;
    void *parameters;
    QueueHandle_t *queue;
    UBaseType_t queue_size;
    UBaseType_t item_size;
    uint16_t stack_size;
    int8_t priority;
    bool is_relative_priority;
} ysw_task_config_t;

typedef void (*ysw_task_event_handler)(void *caller_context, ysw_event_t *event);

void ysw_task_create(ysw_task_config_t *task_config);

void ysw_task_create_standard(char *name, TaskFunction_t function, QueueHandle_t *queue, UBaseType_t item_size);

QueueHandle_t ysw_task_create_event_task(ysw_task_event_handler event_handler, void *caller_context);
