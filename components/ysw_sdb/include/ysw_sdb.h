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
#include "ysw_cn.h"
#include "ysw_cs.h"

typedef void (*ysw_sdb_string_cb_t)(void *context, const char *new_value);
typedef void (*ysw_sdb_choice_cb_t)(void *context, uint8_t new_index);
typedef void (*ysw_sdb_switch_cb_t)(void *context, bool new_value);
typedef void (*ysw_sdb_checkbox_cb_t)(void *context, bool new_value);
typedef void (*ysw_sdb_button_cb_t)(void *context);

typedef struct {
    lv_obj_t *win;
    lv_obj_t *kb; // shared across ta's
    void *context;
} ysw_sdb_t;

ysw_sdb_t *ysw_sdb_create(const char *title, void *context);
void ysw_sdb_add_separator(ysw_sdb_t *sdb, const char *name);
void ysw_sdb_add_string(ysw_sdb_t *sdb, const char *name, const char *value, void *cb);
void ysw_sdb_add_choice(ysw_sdb_t *sdb, const char *name, uint8_t value, const char *options, void *cb);
void ysw_sdb_add_switch(ysw_sdb_t *sdb, const char *name, bool value, void *cb);
void ysw_sdb_add_checkbox(ysw_sdb_t *sdb, const char *name, bool value, void *cb);
void ysw_sdb_add_button(ysw_sdb_t *sdb, const char *name, void *cb);
void ysw_sdb_close(ysw_sdb_t *sdb);
