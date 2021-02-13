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

const ysw_menu_softkey_t ysw_app_softkey_map[] = {
    YSW_R1_C1, YSW_R1_C2, YSW_R1_C3, YSW_R1_C4, YSW_MENU_ENDLINE,
    YSW_R2_C1, YSW_R2_C2, YSW_R2_C3, YSW_R2_C4, YSW_MENU_ENDLINE,
    YSW_R3_C1, YSW_R3_C2, YSW_R3_C3, YSW_R3_C4, YSW_MENU_ENDLINE,
    YSW_R4_C1, YSW_R4_C2, YSW_R4_C3, YSW_R4_C4, YSW_MENU_ENDGRID,
};

// This module provides a local event handler for the user interface
// task that calls the lv_task_handler every YSW_APP_POLL_MS.

// LVLG is not thread safe and functions in this module should only
// be called from a single user interface task.

// At one time we avoided global state by returning a control flag
// from the event handler that could terminate the event loop (i.e.
// to allow an app to return to a parent app).

// This worked fine for message handling (e.g. from the bus), but it
// didn't work for touch events (handled within lv_task_handler).

// Now we take advantage of the single task constraint to use a global
// control field that can be set by touch event handlers.

typedef enum {
    YSW_APP_CONTINUE,
    YSW_APP_TERMINATE,
} ysw_app_control_t;

static ysw_app_control_t control = YSW_APP_CONTINUE;

QueueHandle_t ysw_app_create_queue()
{
    return C(xQueueCreate( ysw_task_default_config.queue_size, ysw_task_default_config.item_size));
}

void ysw_app_handle_events(QueueHandle_t queue, ysw_app_event_handler_t process_event, void *context)
{
    TickType_t wait_ticks = ysw_millis_to_rtos_ticks(YSW_APP_POLL_MS);
    while (control == YSW_APP_CONTINUE) {
        ysw_event_t event;
        BaseType_t is_message = xQueueReceive(queue, &event, wait_ticks);
        if (is_message) {
            process_event(context, &event);
        }
        lv_task_handler();
    }
    control = YSW_APP_CONTINUE; // allow parent app to continue
}

void ysw_app_terminate() {
    control = YSW_APP_TERMINATE;
}
