#pragma once

#include "ysw_array.h"
#include "FreeRTOS.h"
#include "pthread.h"

#define errQUEUE_FULL 0 // see /esp/esp-idf-v4.0/components/freertos/include/freertos/projdefs.h

typedef struct {
    void *data;
    uint32_t read_index;
    uint32_t write_index;
    uint32_t item_size;
    uint32_t queue_length;
    pthread_mutex_t mutex;
    pthread_cond_t data_ready;
    pthread_cond_t space_ready;
} Queue_t;

typedef Queue_t *QueueHandle_t;

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);
BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait);
BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait);
