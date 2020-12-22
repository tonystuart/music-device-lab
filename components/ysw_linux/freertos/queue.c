// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "queue.h"
#include "ysw_heap.h"
#include "esp_log.h"
#include "assert.h"
#include "errno.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "time.h"

#define TAG "QUEUE"

static void increment_timespec(struct timespec *t, uint32_t millis)
{
    int64_t nanos = t->tv_nsec + ((int64_t)millis * 1000000);
    t->tv_sec += nanos / 1000000000;
    t->tv_nsec = nanos % 1000000000;
}

static bool wait_for_cond(pthread_cond_t *cond, pthread_mutex_t *mutex, TickType_t xTicksToWait)
{
    bool ready = true;
    if (xTicksToWait == portMAX_DELAY) {
        pthread_cond_wait(cond, mutex);
    } else {
        struct timespec abstime;
        clock_gettime(CLOCK_REALTIME, &abstime);
        increment_timespec(&abstime, xTicksToWait);
        int rc = pthread_cond_timedwait(cond, mutex, &abstime);
        if (rc == ETIMEDOUT) {
            ready = false;
        } else if (rc != 0) {
            ESP_LOGE(TAG, "pthread_cond_timedwait rc=%d", rc);
        }
    }
    return ready;
}

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize)
{
    assert(uxQueueLength > 0);
    assert(uxItemSize > 0);
    QueueHandle_t xQueue = ysw_heap_allocate(sizeof(Queue_t));
    xQueue->data = ysw_heap_allocate(uxQueueLength * uxItemSize);
    xQueue->item_size = uxItemSize;
    xQueue->queue_length = uxQueueLength;
    pthread_mutex_init(&xQueue->mutex, NULL);
    pthread_cond_init(&xQueue->data_ready, NULL);
    return xQueue;
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait)
{
    assert(xQueue);
    assert(pvItemToQueue);
    pthread_mutex_lock(&xQueue->mutex);
    uint32_t new_write_index = (xQueue->write_index + 1) % xQueue->queue_length;
    if (new_write_index == xQueue->read_index) {
        if (!wait_for_cond(&xQueue->space_ready, &xQueue->mutex, xTicksToWait)) {
            ESP_LOGW(TAG, "queue is full");
            pthread_mutex_unlock(&xQueue->mutex);
            return errQUEUE_FULL;
        }
    }
    memcpy(xQueue->data + (new_write_index * xQueue->item_size), pvItemToQueue, xQueue->item_size);
    xQueue->write_index = new_write_index;
    pthread_cond_signal(&xQueue->data_ready);
    pthread_mutex_unlock(&xQueue->mutex);
    return true;
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait)
{
    assert(xQueue);
    assert(pvBuffer);
    pthread_mutex_lock(&xQueue->mutex);
    while (xQueue->read_index == xQueue->write_index) {
        if (!wait_for_cond(&xQueue->data_ready, &xQueue->mutex, xTicksToWait)) {
            pthread_mutex_unlock(&xQueue->mutex);
            return false;
        }
    }
    xQueue->read_index = (xQueue->read_index + 1) % xQueue->queue_length;
    memcpy(pvBuffer, xQueue->data + (xQueue->read_index * xQueue->item_size), xQueue->item_size);
    pthread_mutex_unlock(&xQueue->mutex);
    return true;
}

BaseType_t xQueueReset(QueueHandle_t xQueue)
{
    assert(xQueue);
    pthread_mutex_lock(&xQueue->mutex);
    xQueue->read_index = 0;
    xQueue->write_index = 0;
    pthread_mutex_unlock(&xQueue->mutex);
    return true;
}

void vQueueDelete(QueueHandle_t xQueue)
{
    pthread_cond_destroy(&xQueue->data_ready);
    pthread_mutex_destroy(&xQueue->mutex);
    ysw_heap_free(xQueue->data);
    ysw_heap_free(xQueue);
}

