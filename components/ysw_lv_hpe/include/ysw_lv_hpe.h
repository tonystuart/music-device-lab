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
#include "ysw_hp.h"

typedef void (*ysw_lv_hpe_select_cb_t)(void *context, ysw_step_t *step);
typedef void (*ysw_lv_hpe_deselect_cb_t)(void *context, ysw_step_t *step);
typedef void (*ysw_lv_hpe_create_cb_t)(void *context, uint32_t step_index, uint8_t degree);
typedef void (*ysw_lv_hpe_drag_end_cb_t)(void *context);

typedef struct {
    ysw_hp_t *hp;
    ysw_step_t *selected_step; // only set while pressed
    ysw_hp_t *drag_start_hp;
    lv_coord_t scroll_left;
    lv_coord_t drag_start_scroll_left;
    lv_point_t last_click;
    bool dragging;
    bool long_press;
    int32_t metro_marker;
    const lv_style_t *bg_style; // background
    const lv_style_t *fg_style; // foreground
    const lv_style_t *rs_style; // regular step
    const lv_style_t *ss_style; // selected step
    const lv_style_t *ms_style; // metro step
    ysw_lv_hpe_create_cb_t create_cb;
    ysw_lv_hpe_select_cb_t select_cb;
    ysw_lv_hpe_deselect_cb_t deselect_cb;
    ysw_lv_hpe_drag_end_cb_t drag_end_cb;
    void *context;
} ysw_lv_hpe_ext_t;

typedef struct {
    bool auto_scroll;
    bool auto_play;
} ysw_lv_hpe_gs_t;

extern ysw_lv_hpe_gs_t ysw_lv_hpe_gs;

lv_obj_t *ysw_lv_hpe_create(lv_obj_t *par, void *context);
void ysw_lv_hpe_set_create_cb(lv_obj_t *hpe, void *cb);
void ysw_lv_hpe_set_select_cb(lv_obj_t *hpe, void *cb);
void ysw_lv_hpe_set_deselect_cb(lv_obj_t *hpe, void *cb);
void ysw_lv_hpe_set_drag_end_cb(lv_obj_t *hpe, void *cb);
void ysw_lv_hpe_set_hp(lv_obj_t *hpe, ysw_hp_t *hp);
void ysw_lv_hpe_on_metro(lv_obj_t *hpe, ysw_note_t *metro_note);
void ysw_lv_hpe_ensure_visible(lv_obj_t *hpe, uint32_t first_step_index, uint32_t last_step_index);

