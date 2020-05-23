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
#include "ysw_cn.h"
#include "ysw_cs.h"
#include "ysw_bounds.h"

typedef enum {
    YSW_LV_CSE_SELECT,
    YSW_LV_CSE_DESELECT,
    YSW_LV_CSE_DRAG_END,
    YSW_LV_CSE_CREATE,
} ysw_lv_cse_event_t;

typedef struct {
    ysw_cn_t *cn;
} ysw_lv_cse_select_t;

typedef struct {
    ysw_cn_t *cn;
} ysw_lv_cse_deselect_t;

typedef struct {
    uint32_t start;
    int8_t degree;
} ysw_lv_cse_create_t;

typedef struct {
    union {
        ysw_lv_cse_select_t select;
        ysw_lv_cse_deselect_t deselect;
        ysw_lv_cse_create_t create;
    };
} ysw_lv_cse_event_cb_data_t;

typedef void (*ysw_lv_cse_event_cb_t)(lv_obj_t *ysw_lv_cse, ysw_lv_cse_event_t event, ysw_lv_cse_event_cb_data_t *data);

typedef struct {
    // TODO: Rename / reorder by role (e.g. dragging)
    ysw_cs_t *cs;
    lv_point_t last_click;
    ysw_cn_t *selected_cn;
    ysw_bounds_t selection_type;
    ysw_cs_t *drag_start_cs;
    bool dragging;
    bool long_press;
    note_t *metro_note;
    const lv_style_t *bg_style; // background
    const lv_style_t *oi_style; // odd interval
    const lv_style_t *ei_style; // even interval
    const lv_style_t *rn_style; // regular note
    const lv_style_t *sn_style; // selected note
    const lv_style_t *mn_style; // metro note
    ysw_lv_cse_event_cb_t event_cb;
} ysw_lv_cse_ext_t;

lv_obj_t *ysw_lv_cse_create(lv_obj_t *par);
void ysw_lv_cse_set_cs(lv_obj_t *cse, ysw_cs_t *cs);
void ysw_lv_cse_select(lv_obj_t *cse, ysw_cn_t *cn, bool is_selected);
bool ysw_lv_cse_is_selected(lv_obj_t *cse, ysw_cn_t *cn);
void ysw_lv_cse_set_event_cb(lv_obj_t *cse, ysw_lv_cse_event_cb_t event_cb);
void ysw_lv_cse_on_metro(lv_obj_t *cse, note_t *metro_note);
