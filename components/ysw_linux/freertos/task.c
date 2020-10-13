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
#include <unistd.h>

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
