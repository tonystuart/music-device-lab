// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_display.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_midi.h"
#include "ysw_staff.h"
#include "ysw_task.h"
#include "zm_music.h"
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "assert.h"
#include "fcntl.h"
#include "limits.h"
#include "stdlib.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"

#define TAG "DISPLAY"

typedef struct {
    lv_obj_t *staff;
} context_t;

static void on_play(context_t *context, ysw_event_play_t *event)
{
    ysw_staff_set_notes(context->staff, event->clip.notes);
}

static void process_event(void *caller_context, ysw_event_t *event)
{
    context_t *context = caller_context;
    if (event) {
        switch (event->header.type) {
            case YSW_EVENT_PLAY:
                on_play(context, &event->play);
            case YSW_EVENT_NOTE_ON:
                break;
            default:
                break;
        }
    }
    lv_task_handler();
}

void ysw_display_create_task(ysw_bus_h bus)
{
    context_t *context = ysw_heap_allocate(sizeof(context_t));

#if 1
    context->staff = ysw_staff_create(lv_scr_act());
    lv_obj_set_size(context->staff, 320, 240);
    lv_obj_align(context->staff, NULL, LV_ALIGN_CENTER, 0, 0);
#else
    extern void lv_demo_widgets(void);
    lv_demo_widgets();
#endif

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.event_handler = process_event;
    config.caller_context = context;
    config.wait_ticks = 5;

    ysw_task_h task = ysw_task_create(&config);
    ysw_task_subscribe(task, YSW_ORIGIN_COMMAND);
    ysw_task_subscribe(task, YSW_ORIGIN_SEQUENCER);
}
