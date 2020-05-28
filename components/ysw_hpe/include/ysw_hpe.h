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
#include "ysw_hp.h"
#include "ysw_cs.h"
#include "ysw_bounds.h"

typedef struct {
    int16_t row;
    int16_t column;
    int16_t old_step_index;
    bool is_cell_edit;
} selection_t;

typedef struct {
    ysw_hp_t *hp;
    lv_obj_t *table;
    lv_obj_t *ddlist;
    selection_t selection;
} ysw_hpe_t;

typedef enum {
    YSW_HPE_SELECT,
    YSW_HPE_DESELECT,
    YSW_HPE_DRAG_END,
    YSW_HPE_CREATE,
} ysw_hpe_event_t;

typedef struct {
    ysw_cs_t *cs;
} ysw_hpe_select_t;

typedef struct {
    ysw_cs_t *cs;
} ysw_hpe_deselect_t;

typedef struct {
    uint32_t start;
    int8_t degree;
} ysw_hpe_create_t;

typedef struct {
    union {
        ysw_hpe_select_t select;
        ysw_hpe_deselect_t deselect;
        ysw_hpe_create_t create;
    };
} ysw_hpe_event_cb_data_t;

typedef void (*ysw_hpe_event_cb_t)(lv_obj_t *ysw_hpe, ysw_hpe_event_t event, ysw_hpe_event_cb_data_t *data);

typedef struct {
    ysw_hp_t *hp;
    ysw_hpe_event_cb_t event_cb;
} ysw_hpe_ext_t;

ysw_hpe_t *ysw_hpe_create(lv_obj_t *par);
void ysw_hpe_set_hp(ysw_hpe_t *hpe, ysw_hp_t *hp);
void ysw_hpe_select(ysw_hpe_t *hpe, ysw_cs_t *cs, bool is_selected);
bool ysw_hpe_is_selected(ysw_hpe_t *hpe, ysw_cs_t *cs);
void ysw_hpe_set_event_cb(ysw_hpe_t *hpe, ysw_hpe_event_cb_t event_cb);
