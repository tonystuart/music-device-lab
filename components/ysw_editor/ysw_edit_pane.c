// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_edit_pane.h"
#include "ysw_heap.h"
#include "ysw_style.h"
#include "esp_log.h"

#define TAG "YSW_EDIT_PANE"

static void on_keyboard_event(lv_obj_t *keyboard, lv_event_t event)
{
    lv_keyboard_def_event_cb(keyboard, event);
    if (event == LV_EVENT_CANCEL) {
        ysw_edit_pane_t *edit_pane = lv_obj_get_user_data(keyboard);
        lv_keyboard_set_textarea(keyboard, NULL);
        lv_obj_del(edit_pane->container);
        edit_pane->keyboard = NULL;
    } else if (event == LV_EVENT_APPLY) {
        ysw_edit_pane_t *edit_pane = lv_obj_get_user_data(keyboard);
        const char *text = lv_textarea_get_text(edit_pane->textarea);
        edit_pane->callback(edit_pane->context, text);
        lv_keyboard_set_textarea(keyboard, NULL);
        lv_obj_del(edit_pane->container);
        edit_pane->keyboard = NULL;
    }
}

static void create_keyboard(ysw_edit_pane_t *edit_pane)
{
    edit_pane->keyboard = lv_keyboard_create(edit_pane->container, NULL);
    lv_obj_set_user_data(edit_pane->keyboard, edit_pane);
    lv_keyboard_set_cursor_manage(edit_pane->keyboard, true);
    lv_obj_set_event_cb(edit_pane->keyboard, on_keyboard_event);
    lv_keyboard_set_textarea(edit_pane->keyboard, edit_pane->textarea);
}

static void on_textarea_event(lv_obj_t *textarea, lv_event_t event)
{
    ysw_edit_pane_t *edit_pane = lv_obj_get_user_data(textarea);
    if (event == LV_EVENT_CLICKED && edit_pane->keyboard == NULL) {
        create_keyboard(edit_pane);
    }
}

ysw_edit_pane_t *ysw_edit_pane_create(char *title, char *text, ysw_edit_pane_cb_t callback, void *context)
{
    ysw_edit_pane_t *edit_pane = ysw_heap_allocate(sizeof(ysw_edit_pane_t));
    edit_pane->callback = callback;
    edit_pane->context = context;
    //edit_pane->container = lv_obj_create(lv_scr_act(), NULL);
    edit_pane->container = lv_cont_create(lv_scr_act(), NULL);
    lv_obj_set_size(edit_pane->container, 320, 240);
    lv_obj_align(edit_pane->container, NULL, LV_ALIGN_CENTER, 0, 0);

    edit_pane->label = lv_label_create(edit_pane->container, NULL);
    lv_label_set_long_mode(edit_pane->label, LV_LABEL_LONG_CROP);
    lv_label_set_align(edit_pane->label, LV_LABEL_ALIGN_CENTER);
    lv_obj_set_width(edit_pane->label, 310);
    //lv_obj_align(edit_pane->container, NULL, LV_ALIGN_IN_TOP_LEFT, 5, 5);
    lv_label_set_text(edit_pane->label, title);

    edit_pane->textarea  = lv_textarea_create(edit_pane->container, NULL);
    lv_obj_set_size(edit_pane->textarea, 310, 85);
    //lv_obj_align(edit_pane->label, NULL, LV_ALIGN_OUT_BOTTOM_LEFT, 5, 5);
    lv_obj_set_user_data(edit_pane->textarea, edit_pane);
    lv_obj_set_event_cb(edit_pane->textarea, on_textarea_event);
    lv_textarea_set_text(edit_pane->textarea, text);

    create_keyboard(edit_pane);

    ysw_style_edit_pane(edit_pane->container, edit_pane->textarea, edit_pane->keyboard);

    lv_cont_set_layout(edit_pane->container, LV_LAYOUT_COLUMN_MID);
    return edit_pane;
}
