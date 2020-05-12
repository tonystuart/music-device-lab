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

typedef void (*ysw_sdb_string_cb_t)(const char *new_value);
typedef void (*ysw_sdb_choice_cb_t)(uint8_t new_value);

typedef struct {
    lv_obj_t *win;
    lv_obj_t *kb; // shared across ta's
} ysw_sdb_t;

ysw_sdb_t *ysw_sdb_create(const char *title);
void ysw_sdb_add_string(ysw_sdb_t *sdb, ysw_sdb_string_cb_t cb, const char *name, const char *value);
void ysw_sdb_add_choice(ysw_sdb_t *sdb, ysw_sdb_choice_cb_t cb, const char *name, uint8_t value, const char *options);
void ysw_sdb_close(ysw_sdb_t *sdb);
