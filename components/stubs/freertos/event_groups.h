#pragma once

#include "pthread.h"
#include "stdint.h"
#include "FreeRTOS.h"

typedef uint32_t EventBits_t;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t bits_ready;
    EventBits_t bits;
} EventGroup_t;

typedef EventGroup_t *EventGroupHandle_t;

EventGroupHandle_t xEventGroupCreate();

EventBits_t xEventGroupWaitBits(
        const EventGroupHandle_t xEventGroup,
        const EventBits_t uxBitsToWaitFor,
        const BaseType_t xClearOnExit,
        const BaseType_t xWaitForAllBits,
        TickType_t xTicksToWait);

EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup,
        const EventBits_t uxBitsToSet);

void vEventGroupDelete(EventGroupHandle_t xEventGroup);

