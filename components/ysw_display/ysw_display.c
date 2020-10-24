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

// lv_label selection is currently broken:
// https://github.com/lvgl/lvgl/issues/1820

#define USE_RECOLOR_SELECTION 1

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
}

static void on_passage_update(context_t *context, ysw_event_passage_update_t *event)
{
    uint32_t beat_count = ysw_array_get_count(event->passage->beats);
    uint32_t symbol_count = beat_count * 2;
    uint32_t size = (symbol_count * 2) + 20; // * 2 for max strlen(lookup[i]) and + 10 for prefix + color
    char buffer[size];
    char *p = buffer;
    *p++ = '\'';
    *p++ = '&';
    *p++ = '='; // key signature
    *p++ = '4'; // time signature
    *p++ = '=';

#if USE_RECOLOR_SELECTION
#else
    uint32_t sel_start = LV_DRAW_LABEL_NO_TXT_SEL;
    uint32_t sel_end = LV_DRAW_LABEL_NO_TXT_SEL;
#endif

    for (int i = 0; i <= symbol_count; i++) { // = to get trailing space
        if (i == event->position) {
#if USE_RECOLOR_SELECTION
            const char *q = "#ff0000 ";
            while (*q) {
                *p++ = *q++;
            }
#else
            sel_start = p - buffer;
#endif
        }
        if (i % 2 == 0) {
            *p++ = '=';
        } else {
            uint32_t beat_index = i / 2;
            zm_beat_t *beat = ysw_array_get(event->passage->beats, beat_index);
            if (beat->tone.note) {
                const char *q = lookup[beat->tone.note - 60];
                while (*q) {
                    *p++ = *q++;
                }
            } else {
                // rest
            }
        }
        if (i == event->position) {
#if USE_RECOLOR_SELECTION
            *p++ = '#';
#else
            sel_end = p - buffer;
#endif
        }
    }
    *p = 0;
    ESP_LOGD(TAG, "buffer=%s", buffer);
    lv_label_set_text(context->label, buffer);
#if USE_RECOLOR_SELECTION
    lv_coord_t width = lv_obj_get_width(context->label);
    lv_coord_t dist = max(width - 180, 0);
    ESP_LOGD(TAG, "width=%d, dist=%d", width, dist);
    //lv_page_scroll_hor(context->page, -dist);
    lv_obj_t *scrl = lv_page_get_scrollable(context->page);
    lv_obj_set_x(scrl, lv_obj_get_x(scrl) - dist);
#else 
    lv_label_set_text_sel_start(context->label, sel_start);
    lv_label_set_text_sel_end(context->label, sel_end);
#endif
}

static void process_event(void *caller_context, ysw_event_t *event)
{
    context_t *context = caller_context;
    if (event) {
        switch (event->header.type) {
            case YSW_EVENT_PASSAGE_UPDATE:
                on_passage_update(context, &event->passage_update);
                break;
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

    context->page = lv_page_create(lv_scr_act(), NULL);
    lv_obj_set_size(context->page, 320, 240);
    lv_obj_align(context->page, NULL, LV_ALIGN_CENTER, 0, 0);

    context->label = lv_label_create(context->page, NULL);
    lv_obj_set_style_local_text_font(context->label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, &MusiQwik_48);
    lv_obj_set_style_local_text_sel_color(context->label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GREEN);
    lv_label_set_recolor(context->label, true);
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
    ysw_task_subscribe(task, YSW_ORIGIN_EDITOR);
    ysw_task_subscribe(task, YSW_ORIGIN_COMMAND);
    ysw_task_subscribe(task, YSW_ORIGIN_NOTE);
}
