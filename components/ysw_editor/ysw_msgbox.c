// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_msgbox.h"
#include "ysw_common.h"
#include "ysw_heap.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_MSGBOX"

#define OKAY "OK"
#define CANCEL "Cancel"

static void event_handler(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        ysw_msgbox_t *msgbox = lv_obj_get_user_data(obj);
        assert(msgbox);
        const char *btn_text = lv_msgbox_get_active_btn_text(obj);
        assert(btn_text);
        if (strcmp(btn_text, OKAY) == 0) {
            if (msgbox->on_okay) {
                msgbox->on_okay(msgbox->context);
            }
        } else if (strcmp(btn_text, CANCEL) == 0) {
            if (msgbox->on_cancel) {
                msgbox->on_cancel(msgbox->context);
            }
        }
    }
}

ysw_msgbox_t *ysw_msgbox_create(ysw_msgbox_config_t *config, void *context)
{
    ysw_msgbox_t *msgbox = ysw_heap_allocate(sizeof(ysw_msgbox_t));
    msgbox->context = context; // TODO: consider initializing via config

    switch (config->type) {
        case YSW_MSGBOX_OKAY:
            msgbox->buttons = (const char *[]) { OKAY, "" };
            msgbox->on_okay = config->on_okay;
            break;
        case YSW_MSGBOX_OKAY_CANCEL:
            msgbox->buttons = (const char *[]) { OKAY, CANCEL, "" };
            msgbox->on_okay = config->on_okay;
            msgbox->on_cancel = config->on_cancel;
            break;
        default:
            break;
    }

    msgbox->popup = lv_msgbox_create(lv_scr_act(), NULL);
    lv_msgbox_set_text(msgbox->popup, config->message);
    lv_msgbox_add_btns(msgbox->popup, msgbox->buttons);
    lv_obj_set_width(msgbox->popup, 280);
    lv_obj_align(msgbox->popup, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_user_data(msgbox->popup, msgbox);
    lv_obj_set_event_cb(msgbox->popup, event_handler);

    return msgbox;
}

void ysw_msgbox_free(ysw_msgbox_t *msgbox)
{
    // msgbox has a default event callback that closes itself when a button is clicked
    ysw_heap_free(msgbox);
}

void ysw_msgbox_on_key_down(ysw_msgbox_t *msgbox, ysw_event_t *event)
{
    ESP_LOGD(TAG, "ysw_msgbox_on_key_down, ignoring event");
}


