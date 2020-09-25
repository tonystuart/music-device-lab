// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "freertos/FreeRTOS.h"
#include "semphr.h"
#include "ysw_heap.h"
#include "esp_log.h"
#include "semaphore.h"
#include "stdlib.h"

#define TAG "SEMPHR"

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    sem_t *sem = ysw_heap_allocate(sizeof(sem_t));
    sem_init(sem, 0, 1);
    return sem;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait)
{
    int rc = sem_wait(xSemaphore);
    if (rc == -1) {
        ESP_LOGE(TAG, "sem_wait failed");
        abort();
    }
    return true;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
    int rc = sem_post(xSemaphore);
    if (rc == -1) {
        ESP_LOGE(TAG, "sem_post failed");
        abort();
    }
    return true;
}

void vSemaphoreDelete(SemaphoreHandle_t xSemaphore)
{
    int rc = sem_destroy(xSemaphore);
    if (rc == -1) {
        ESP_LOGE(TAG, "sem_destroy failed");
        abort();
    }
    ysw_heap_free(xSemaphore);
}
