// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_app.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_task.h"
#include "lvgl.h"
#include "esp_log.h"

#define TAG "YSW_QUEUE"

#define YSW_APP_POLL_MS 5

// Layout of keycodes generated by keyboard:
//
//    0,  1,      2,  3,  4,      5,  6,  7,  8,
//  9, 10, 11, 12, 13, 14, 15,   16, 17, 18, 19,
//   20, 21,     22, 23, 24,     25, 26, 27, 28,
// 29, 30, 31, 32, 33, 34, 35,   36, 37, 38, 39,

const ysw_menu_softkey_t ysw_app_softkey_map[] = {
    5, 6, 7, 8, YSW_MENU_ENDLINE,
    16, 17, 18, 19, YSW_MENU_ENDLINE,
    25, 26, 27, 28, YSW_MENU_ENDLINE,
    36, 37, 38, 39, YSW_MENU_ENDGRID,
};

QueueHandle_t ysw_app_create_queue()
{
    return C(xQueueCreate( ysw_task_default_config.queue_size, ysw_task_default_config.item_size));
}

void ysw_app_handle_events(QueueHandle_t queue, ysw_app_event_handler_t process_event, void *context)
{
    ysw_app_control_t control = YSW_APP_CONTINUE;
    TickType_t wait_ticks = ysw_millis_to_rtos_ticks(YSW_APP_POLL_MS);
    while (control == YSW_APP_CONTINUE) {
        ysw_event_t event;
        BaseType_t is_message = xQueueReceive(queue, &event, wait_ticks);
        if (is_message) {
            control = process_event(context, &event);
        } 
        lv_task_handler();
    }
}

