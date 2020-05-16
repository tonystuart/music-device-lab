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
#include "ysw_array.h"
#include "ysw_cp.h"
#include "ysw_cs.h"
#include "ysw_bounds.h"

typedef struct {
    int16_t row;
    int16_t column;
    int16_t old_step_index;
    bool is_cell_edit;
} selection_t;

typedef struct {
    ysw_cp_t *cp;
    lv_obj_t *table;
    lv_obj_t *ddlist;
    selection_t selection;
} ysw_cpe_t;

typedef enum {
    YSW_CPE_SELECT,
    YSW_CPE_DESELECT,
    YSW_CPE_DRAG_END,
    YSW_CPE_CREATE,
} ysw_cpe_event_t;

typedef struct {
    ysw_cs_t *cs;
} ysw_cpe_select_t;

typedef struct {
    ysw_cs_t *cs;
} ysw_cpe_deselect_t;

typedef struct {
    uint32_t start;
    int8_t degree;
} ysw_cpe_create_t;

typedef struct {
    union {
        ysw_cpe_select_t select;
        ysw_cpe_deselect_t deselect;
        ysw_cpe_create_t create;
    };
} ysw_cpe_event_cb_data_t;

typedef void (*ysw_cpe_event_cb_t)(lv_obj_t *ysw_cpe, ysw_cpe_event_t event, ysw_cpe_event_cb_data_t *data);

typedef struct {
    ysw_cp_t *cp;
    ysw_cpe_event_cb_t event_cb;
} ysw_cpe_ext_t;

ysw_cpe_t *ysw_cpe_create(lv_obj_t *par);
void ysw_cpe_set_cp(ysw_cpe_t *cpe, ysw_cp_t *cp);
void ysw_cpe_select(ysw_cpe_t *cpe, ysw_cs_t *cs, bool is_selected);
bool ysw_cpe_is_selected(ysw_cpe_t *cpe, ysw_cs_t *cs);
void ysw_cpe_set_event_cb(ysw_cpe_t *cpe, ysw_cpe_event_cb_t event_cb);
