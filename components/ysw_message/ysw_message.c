// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_message.h"
#include "stdlib.h"

#define TAG "YSW_MESSAGE"
#define MAX_WAIT_MS 100

void ysw_message_send(QueueHandle_t queue, void *message)
{
    BaseType_t rc;
    do {
        rc = xQueueSend(queue, message, MAX_WAIT_MS / portTICK_PERIOD_MS);
        if (rc == errQUEUE_FULL) {
            ESP_LOGW(TAG, "xQueueSend failed, queue is full, retrying");
        }
    } while (rc != pdTRUE);
}

void ysw_message_initiate_rendezvous(QueueHandle_t queue, void *data)
{
    EventGroupHandle_t event = xEventGroupCreate();
    if (!event) {
        ESP_LOGE(TAG, "xEventGroupCreate failed");
        abort();
    }

    ysw_message_rendezvous_t rendezvous = {
        .event = event,
        .data = data,
    };

    ESP_LOGD(TAG, "initiate_rendezvous sending message");
    ysw_message_send(queue, &rendezvous);

    ESP_LOGD(TAG, "initiate_rendezvous waiting for notification");
    xEventGroupWaitBits(rendezvous.event, 1, false, true, portMAX_DELAY);

    ESP_LOGD(TAG, "initiate_rendezvous deleting event group");
    vEventGroupDelete(rendezvous.event);
}

void ysw_message_complete_rendezvous(ysw_message_rendezvous_t *rendezvous)
{
    ESP_LOGD(TAG, "complete_rendezvous notifying sender");
    xEventGroupSetBits(rendezvous->event, 1);
}

