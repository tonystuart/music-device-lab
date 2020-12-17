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

typedef void (*ysw_msgbox_cb_t)(void *context);

typedef enum {
    YSW_MSGBOX_OKAY,
    YSW_MSGBOX_OKAY_CANCEL,
} ysw_msgbox_type_t;

typedef struct {
    void *context;
    ysw_msgbox_type_t type;
    const char *message;
    ysw_msgbox_cb_t on_okay;
    ysw_msgbox_cb_t on_cancel;
    uint8_t okay_key;
    uint8_t cancel_key;
} ysw_msgbox_config_t;

typedef struct {
    void *context;
    lv_obj_t *popup;
    const char **buttons;
    ysw_msgbox_cb_t on_okay;
    ysw_msgbox_cb_t on_cancel;
    uint8_t okay_key;
    uint8_t cancel_key;
} ysw_msgbox_t;

ysw_msgbox_t *ysw_msgbox_create(ysw_msgbox_config_t *config);
void ysw_msgbox_free(ysw_msgbox_t *msgbox);
void ysw_msgbox_on_key_down(ysw_msgbox_t *msgbox, ysw_event_t *event);
