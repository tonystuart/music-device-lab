// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_popup.h"
#include "ysw_common.h"
#include "ysw_heap.h"
#include "ysw_style.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_MSGBOX"

#define OKAY "OK"
#define YES "Yes"
#define NO "No"
#define CANCEL "Cancel"

static const char *MSGBOX_OKAY[] = {
    OKAY, "",
};

static const char *MSGBOX_OKAY_CANCEL[] = {
    OKAY, CANCEL, "",
};

static const char *MSGBOX_YES_NO_CANCEL[] = {
    YES, NO, CANCEL, "",
};

static void event_handler(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        ysw_popup_t *popup = lv_obj_get_user_data(obj);
        assert(popup);
        const char *btn_text = lv_msgbox_get_active_btn_text(obj);
        assert(btn_text);
        if (strcmp(btn_text, OKAY) == 0) {
            if (popup->on_okay) {
                popup->on_okay(popup->context, popup);
            }
        } else if (strcmp(btn_text, YES) == 0) {
            if (popup->on_yes) {
                popup->on_yes(popup->context, popup);
            }
        } else if (strcmp(btn_text, NO) == 0) {
            if (popup->on_no) {
                popup->on_no(popup->context, popup);
            }
        } else if (strcmp(btn_text, CANCEL) == 0) {
            if (popup->on_cancel) {
                popup->on_cancel(popup->context, popup);
            }
        }
        lv_obj_del(popup->container);
        ysw_heap_free(popup);
    }
}

void ysw_popup_create(ysw_popup_config_t *config)
{
    ysw_popup_t *popup = ysw_heap_allocate(sizeof(ysw_popup_t));
    popup->context = config->context;
    popup->okay_scan_code = config->okay_scan_code;
    popup->cancel_scan_code = config->cancel_scan_code;

    switch (config->type) {
        case YSW_MSGBOX_OKAY:
            popup->buttons = MSGBOX_OKAY;
            popup->on_okay = config->on_okay;
            break;
        case YSW_MSGBOX_OKAY_CANCEL:
            popup->buttons = MSGBOX_OKAY_CANCEL;
            popup->on_okay = config->on_okay;
            popup->on_cancel = config->on_cancel;
            break;
        case YSW_MSGBOX_YES_NO_CANCEL:
            popup->buttons = MSGBOX_YES_NO_CANCEL;
            popup->on_yes = config->on_yes;
            popup->on_no = config->on_no;
            popup->on_cancel = config->on_cancel;
            break;
        default:
            break;
    }

    popup->container = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(popup->container, 320, 240);
    lv_obj_align(popup->container, NULL, LV_ALIGN_CENTER, 0, 0);

    popup->msgbox = lv_msgbox_create(popup->container, NULL);
    lv_msgbox_set_text(popup->msgbox, config->message);
    lv_msgbox_add_btns(popup->msgbox, popup->buttons);
    lv_obj_set_width(popup->msgbox, 280);
    lv_obj_align(popup->msgbox, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);
    lv_obj_set_user_data(popup->msgbox, popup);
    lv_obj_set_event_cb(popup->msgbox, event_handler);

    ysw_style_popup(popup->container, popup->msgbox);
}

// TODO: tell menu to send keystrokes to us

void ysw_popup_on_key_down(ysw_popup_t *popup, ysw_event_t *event)
{
    if (event->key_down.scan_code == popup->okay_scan_code) {
        ESP_LOGD(TAG, "okay key");
        if (popup->on_okay) {
            popup->on_okay(popup->context, popup);
        }
    } else if (event->key_down.scan_code == popup->yes_scan_code) {
        ESP_LOGD(TAG, "yes key");
        if (popup->on_yes) {
            popup->on_yes(popup->context, popup);
        }
    } else if (event->key_down.scan_code == popup->no_scan_code) {
        ESP_LOGD(TAG, "no key");
        if (popup->on_no) {
            popup->on_no(popup->context, popup);
        }
    } else if (event->key_down.scan_code == popup->cancel_scan_code) {
        ESP_LOGD(TAG, "cancel key");
        if (popup->on_cancel) {
            popup->on_cancel(popup->context, popup);
        }
    }
}
