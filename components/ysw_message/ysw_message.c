// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_message.h"

#include "freertos/task.h"

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
