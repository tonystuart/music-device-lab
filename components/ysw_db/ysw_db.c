// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_db.h"
#include "ysw_heap.h"
#include "ysw_lv_styles.h"
#include "lvgl.h"
#include "esp_log.h"
#include "stdio.h"

#define TAG "YSW_DB"

#define YSW_DB_OK "OK"
#define YSW_DB_CANCEL "Cancel"

typedef void (*ysw_db_cb_t)(void *context);

typedef struct {
    void *context;
    ysw_db_cb_t cb;
} ysw_db_t;

static void ysw_db_event_cb(lv_obj_t *mbox, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        const char *text = lv_mbox_get_active_btn_text(mbox);
        if (strcmp(text, YSW_DB_OK) == 0) {
            ysw_db_t *ysw_db = lv_obj_get_user_data(mbox);
            ysw_db->cb(ysw_db->context);
            ysw_heap_free(ysw_db);
            lv_mbox_start_auto_close(mbox, 0);
        }
    }
}

void ysw_db_conf_create(const char* text, void *cb, void *context)
{
    static const char *btns[] = { YSW_DB_CANCEL, YSW_DB_OK, "" };
    lv_obj_t *mbox = lv_mbox_create(lv_scr_act(), NULL);
    lv_mbox_set_text(mbox, text);
    lv_mbox_add_btns(mbox, btns);
    lv_obj_set_width(mbox, 300);
    lv_obj_set_event_cb(mbox, ysw_db_event_cb);
    lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
    ysw_db_t *ysw_db = ysw_heap_allocate(sizeof(ysw_db_t));
    ysw_db->context = context;
    ysw_db->cb = cb;
    lv_obj_set_user_data(mbox, ysw_db);
}

