// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

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

