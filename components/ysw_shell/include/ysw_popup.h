// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_event.h"
#include "lvgl.h"

typedef struct ysw_popup_s ysw_popup_t;

typedef void (*ysw_popup_cb_t)(void *context, ysw_popup_t *popup);

typedef enum {
    YSW_MSGBOX_OKAY,
    YSW_MSGBOX_OKAY_CANCEL,
} ysw_popup_type_t;

typedef struct {
    void *context;
    ysw_popup_type_t type;
    const char *message;
    ysw_popup_cb_t on_okay;
    ysw_popup_cb_t on_cancel;
    uint8_t okay_scan_code;
    uint8_t cancel_scan_code;
} ysw_popup_config_t;

typedef struct ysw_popup_s {
    void *context;
    lv_obj_t *container;
    lv_obj_t *msgbox;
    const char **buttons;
    ysw_popup_cb_t on_okay;
    ysw_popup_cb_t on_cancel;
    uint8_t okay_scan_code;
    uint8_t cancel_scan_code;
} ysw_popup_t;

void ysw_popup_create(ysw_popup_config_t *config);
