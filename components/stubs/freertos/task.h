
#pragma once

#define tskIDLE_PRIORITY 0

typedef void *TaskFunction_t;
typedef void *TaskHandle_t;

extern int xTaskGetTickCount();
extern void vTaskDelay(int ticks);
extern char *pcTaskGetTaskName(TaskHandle_t handle);
