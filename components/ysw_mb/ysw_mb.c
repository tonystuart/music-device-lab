// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_mb.h"
#include "ysw_heap.h"
#include "ysw_style.h"
#include "lvgl.h"
#include "esp_log.h"
#include "stdio.h"

#define TAG "YSW_MB"

#define YSW_MB_OK "OK"
#define YSW_MB_CANCEL "Cancel"

typedef void (*ysw_mb_cb_t)(void *context);

typedef struct {
    void *context;
    ysw_mb_cb_t cb;
} ysw_mb_t;

static void ysw_mb_event_cb(lv_obj_t *mbox, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        const char *text = lv_msgbox_get_active_btn_text(mbox);
        if (strcmp(text, YSW_MB_OK) == 0) {
            ysw_mb_t *ysw_mb = lv_obj_get_user_data(mbox);
            ysw_mb->cb(ysw_mb->context);
            ysw_heap_free(ysw_mb);
        }
        lv_msgbox_start_auto_close(mbox, 0);
    }
}

void ysw_mb_create_confirm(const char* text, void *cb, void *context)
{
    static const char *btns[] = { YSW_MB_CANCEL, YSW_MB_OK, "" };
    lv_obj_t *mbox = lv_msgbox_create(lv_scr_act(), NULL);
    lv_msgbox_set_text(mbox, text);
    lv_msgbox_add_btns(mbox, btns);
    lv_obj_set_width(mbox, 300);
    lv_obj_set_event_cb(mbox, ysw_mb_event_cb);
    lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
    ysw_mb_t *ysw_mb = ysw_heap_allocate(sizeof(ysw_mb_t));
    ysw_mb->context = context;
    ysw_mb->cb = cb;
    lv_obj_set_user_data(mbox, ysw_mb);
}

void ysw_mb_create_okay(const char *text)
{
    static const char *btns[] = { YSW_MB_OK, "" };
    lv_obj_t *mbox = lv_msgbox_create(lv_scr_act(), NULL);
    lv_msgbox_set_text(mbox, text);
    lv_msgbox_add_btns(mbox, btns);
    lv_obj_set_width(mbox, 300);
    lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
}

void ysw_mb_nothing_selected()
{
    ysw_mb_create_okay("Please select an item and try again");
}

void ysw_mb_clipboard_empty()
{
    ysw_mb_create_okay("The clipboard is empty");
}

