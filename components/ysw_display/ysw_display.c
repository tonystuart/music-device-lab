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
#include "ysw_staff.h"
#include "ysw_task.h"
#include "lvgl.h"
#include "esp_log.h"

#define TAG "DISPLAY"

typedef struct {
#if 0
    lv_obj_t *staff;
#endif
    lv_obj_t *page;
    lv_obj_t *label;
} context_t;

static void on_play(context_t *context, ysw_event_play_t *event)
{
#if 0
    ysw_staff_set_notes(context->staff, event->clip.notes);
#endif
}

static const char *lookup[] = {
    // C,      #C,   D,        #D,   E,   F,         #F,    G,        #G,   A,        #A,   B
    "R", "\u00d2R", "S", "\u00d3S", "T", "U",  "\u00d5U",  "V", "\u00d6V", "W", "\u00d7W", "X",
    "Y", "\u00d9Y", "Z", "\u00daZ", "[", "\\", "\u00dc\\", "]", "\u00dd]", "^", "\u00de^", "_",
};

static void on_note_on(context_t *context, ysw_event_note_on_t *event)
{
    ESP_LOGD(TAG, "channel=%d, midi_note=%d", event->channel, event->midi_note);
    if (!event->channel && event->midi_note >= 60 && event->midi_note < 84) {
        char *old_text = lv_label_get_text(context->label);
        const char *note_text = lookup[event->midi_note - 60];
        int old_length = strlen(old_text);
        int note_length = strlen(note_text);
        int new_length = old_length + note_length + 1;
        char new_text[new_length];
        strcpy(new_text, old_text);
        strcat(new_text, note_text);
        strcat(new_text, "=");
        lv_label_set_text(context->label, new_text);
        lv_coord_t width = lv_obj_get_width(context->label);
        lv_coord_t dist = max(width - 180, 0);
        ESP_LOGD(TAG, "width=%d, dist=%d", width, dist);
        //lv_page_scroll_hor(context->page, -dist);
        lv_obj_t *scrl = lv_page_get_scrollable(context->page);
        lv_obj_set_x(scrl, lv_obj_get_x(scrl) - dist);
    }
}

static void process_event(void *caller_context, ysw_event_t *event)
{
    context_t *context = caller_context;
    if (event) {
        switch (event->header.type) {
            case YSW_EVENT_PLAY:
                on_play(context, &event->play);
                break;
            case YSW_EVENT_NOTE_ON:
                on_note_on(context, &event->note_on);
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

    /*Create a page*/
    context->page = lv_page_create(lv_scr_act(), NULL);
    lv_obj_set_size(context->page, 320, 240);
    lv_obj_align(context->page, NULL, LV_ALIGN_CENTER, 0, 0);

    /*Create a label on the page*/
    context->label = lv_label_create(context->page, NULL);
    lv_obj_set_style_local_text_font(context->label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, &MusiQwik_48);
    //lv_label_set_long_mode(context->label, LV_LABEL_LONG_BREAK);            /*Automatically break long lines*/
    //lv_obj_set_width(context->label, lv_page_get_width_fit(context->page));          /*Set the label width to max value to not show hor. scroll bars*/
    lv_label_set_text(context->label, "'&=4=");

#if 0
    context->staff = ysw_staff_create(lv_scr_act());
    lv_obj_set_size(context->staff, 320, 240);
    lv_obj_align(context->staff, NULL, LV_ALIGN_CENTER, 0, 0);
#endif
#if 0
    extern void lv_demo_widgets(void);
    lv_demo_widgets();
#endif

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.event_handler = process_event;
    config.caller_context = context;
    config.wait_millis = 5;
    config.priority = YSW_TASK_DEFAULT_PRIORITY + 1;

    ysw_task_h task = ysw_task_create(&config);
    ysw_task_subscribe(task, YSW_ORIGIN_COMMAND);
    ysw_task_subscribe(task, YSW_ORIGIN_SEQUENCER);
}
