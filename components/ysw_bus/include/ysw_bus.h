// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_origin.h"
#include "ysw_pool.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "stdint.h"

typedef struct {
    QueueHandle_t queue;
    uint16_t origins_size;
    uint16_t listeners_size;
    uint16_t bus_message_size;
    ysw_pool_t *listeners[]; // flexible array member
} ysw_bus_t;

ysw_bus_t *ysw_bus_create(uint16_t origins_size, uint16_t listeners_size, uint32_t queue_size, uint32_t message_size);
void ysw_bus_subscribe(ysw_bus_t *bus, ysw_origin_t origin, QueueHandle_t queue);
void ysw_bus_publish(ysw_bus_t *bus, ysw_origin_t origin, void *message, uint32_t length);
void ysw_bus_unsubscribe(ysw_bus_t *bus, ysw_origin_t origin, QueueHandle_t queue);
void ysw_bus_delete_queue(ysw_bus_t *bus, QueueHandle_t queue);
void ysw_bus_free(ysw_bus_t *bus);

