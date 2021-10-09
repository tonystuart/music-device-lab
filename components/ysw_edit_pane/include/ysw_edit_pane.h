// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "lvgl.h"

typedef void (*ysw_edit_pane_cb_t)(void *context, const char *text);

typedef struct {
    void *context;
    lv_obj_t *container;
    lv_obj_t *label;
    lv_obj_t *textarea;
    lv_obj_t *keyboard;
    ysw_edit_pane_cb_t callback;
} ysw_edit_pane_t;

ysw_edit_pane_t *ysw_edit_pane_create(char *title, char *text, ysw_edit_pane_cb_t callback, void *context);
