// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

typedef struct {
    EventGroupHandle_t event;
    void *data;
} ysw_message_rendezvous_t;

void ysw_message_send(QueueHandle_t queue, void *message);
void ysw_message_initiate_rendezvous(QueueHandle_t queue, void *data);
void ysw_message_complete_rendezvous(ysw_message_rendezvous_t *rendezvous);
