// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#define tskIDLE_PRIORITY 0

typedef void *TaskFunction_t;
typedef void *TaskHandle_t;

extern int xTaskGetTickCount();
extern void vTaskDelay(int ticks);
extern char *pcTaskGetTaskName(TaskHandle_t handle);
