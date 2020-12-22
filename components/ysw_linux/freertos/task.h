// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "FreeRTOS.h"

#define tskIDLE_PRIORITY 0

typedef void (*TaskFunction_t)( void * );
typedef void *TaskHandle_t;

extern int xTaskGetTickCount();
extern void vTaskDelay(int ticks);
extern char *pcTaskGetTaskName(TaskHandle_t handle);
void vTaskDelete(TaskHandle_t handle);
UBaseType_t uxTaskPriorityGet(TaskHandle_t xTask);
BaseType_t xTaskCreate(TaskFunction_t pvTaskCode, const char *const pcName, configSTACK_DEPTH_TYPE usStackDepth, void *pvParameters, UBaseType_t uxPriority, TaskHandle_t *pxCreatedTask);
