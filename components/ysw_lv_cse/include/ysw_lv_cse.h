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
#include "ysw_auto_play.h"
#include "ysw_sn.h"
#include "ysw_cs.h"
#include "ysw_bounds.h"

typedef void (*ysw_lv_cse_create_cb_t)(void *context, uint32_t start, int8_t degree);
typedef void (*ysw_lv_cse_edit_cb_t)(void *context, ysw_sn_t *sn);
typedef void (*ysw_lv_cse_select_cb_t)(void *context, ysw_sn_t *sn);
typedef void (*ysw_lv_cse_deselect_cb_t)(void *context, ysw_sn_t *sn);
typedef void (*ysw_lv_cse_drag_end_cb_t)(void *context);

typedef struct {
    // TODO: Rename / reorder by role (e.g. dragging)
    ysw_cs_t *cs;
    lv_point_t click_point;
    ysw_sn_t *clicked_sn; // only set wile pressed
    ysw_bounds_t click_type;
    ysw_cs_t *drag_start_cs;
    bool dragging;
    bool long_press;
    bool press_lost;
    int32_t metro_marker;
    const lv_style_t *bg_style; // background
    const lv_style_t *oi_style; // odd interval
    const lv_style_t *ei_style; // even interval
    const lv_style_t *rn_style; // regular note
    const lv_style_t *sn_style; // selected note
    const lv_style_t *mn_style; // metro note
    ysw_lv_cse_create_cb_t create_cb;
    ysw_lv_cse_edit_cb_t edit_cb;
    ysw_lv_cse_select_cb_t select_cb;
    ysw_lv_cse_deselect_cb_t deselect_cb;
    ysw_lv_cse_drag_end_cb_t drag_end_cb;
    void *context;
} ysw_lv_cse_ext_t;

typedef struct {
    ysw_auto_play_t auto_play;
} ysw_lv_cse_gs_t;

extern ysw_lv_cse_gs_t ysw_lv_cse_gs;

lv_obj_t *ysw_lv_cse_create(lv_obj_t *par, void *context);
void ysw_lv_cse_set_create_cb(lv_obj_t *cse, void *cb);
void ysw_lv_cse_set_edit_cb(lv_obj_t *cse, void *cb);
void ysw_lv_cse_set_select_cb(lv_obj_t *cse, void *cb);
void ysw_lv_cse_set_deselect_cb(lv_obj_t *cse, void *cb);
void ysw_lv_cse_set_drag_end_cb(lv_obj_t *cse, void *cb);
void ysw_lv_cse_set_cs(lv_obj_t *cse, ysw_cs_t *cs);
void ysw_lv_cse_on_metro(lv_obj_t *cse, ysw_note_t *metro_note);
