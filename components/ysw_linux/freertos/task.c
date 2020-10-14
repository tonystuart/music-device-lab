// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "task.h"
#include "ysw_system.h"
#include "esp_log.h"
#include "FreeRTOS.h"
#include "pthread.h"
#include "unistd.h"

#define TAG "TASK"

static int to_millis(int ticks)
{
    return ticks * portTICK_PERIOD_MS;
}

static int to_ticks(int millis)
{
    if (millis && (millis < portTICK_PERIOD_MS)) {
        ESP_LOGW(TAG, "to_ticks millis=%d < portTICK_PERIOD_MS=%d", millis, portTICK_PERIOD_MS);
    }
    return millis / portTICK_PERIOD_MS;
}

int xTaskGetTickCount()
{
    return to_ticks(ysw_system_get_reference_time_in_millis());
}

void vTaskDelay(int ticks)
{
    usleep(to_millis(ticks) * 1000);
}

char *pcTaskGetTaskName(TaskHandle_t handle)
{
    return "TASK_NAME";
}

void vTaskDelete(TaskHandle_t handle)
{
    ESP_LOGW(TAG, "vTaskDelete stub entered");
}

UBaseType_t uxTaskPriorityGet(TaskHandle_t xTask)
{
    ESP_LOGW(TAG, "uxTaskPriorityGet stub entered");
    return 0;
}

BaseType_t xTaskCreate(TaskFunction_t pvTaskCode, const char *const pcName, configSTACK_DEPTH_TYPE usStackDepth, void *pvParameters, UBaseType_t uxPriority, TaskHandle_t *pxCreatedTask)
{
    pthread_t tid;
    int rc = pthread_create(&tid, NULL, pvTaskCode, pvParameters);
    if (rc == 0) {
        rc = pdPASS;
    } else {
        rc = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }
    return rc;
}

