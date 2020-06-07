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
#include "ysw_auto_play.h"
#include "ysw_hp.h"

typedef void (*ysw_lv_hpe_create_cb_t)(void *context, uint32_t ps_index, uint8_t degree);
typedef void (*ysw_lv_hpe_edit_cb_t)(void *context, ysw_ps_t *ps);
typedef void (*ysw_lv_hpe_select_cb_t)(void *context, ysw_ps_t *ps);
typedef void (*ysw_lv_hpe_deselect_cb_t)(void *context, ysw_ps_t *ps);
typedef void (*ysw_lv_hpe_drag_end_cb_t)(void *context);

typedef struct {
    ysw_hp_t *hp;
    ysw_ps_t *clicked_ps; // only set while pressed
    ysw_hp_t *drag_start_hp;
    lv_coord_t scroll_left;
    lv_coord_t drag_start_scroll_left;
    lv_point_t click_point;
    bool dragging;
    bool scrolling;
    bool long_press;
    bool press_lost;
    int32_t metro_marker;
    const lv_style_t *bg_style; // background
    const lv_style_t *fg_style; // foreground
    const lv_style_t *rs_style; // regular ps
    const lv_style_t *ss_style; // selected ps
    const lv_style_t *ms_style; // metro ps
    ysw_lv_hpe_create_cb_t create_cb;
    ysw_lv_hpe_edit_cb_t edit_cb;
    ysw_lv_hpe_select_cb_t select_cb;
    ysw_lv_hpe_deselect_cb_t deselect_cb;
    ysw_lv_hpe_drag_end_cb_t drag_end_cb;
    void *context;
} ysw_lv_hpe_ext_t;

typedef struct {
    bool auto_scroll;
    ysw_auto_play_t auto_play_all;
    ysw_auto_play_t auto_play_last;
    bool multiple_selection;
} ysw_lv_hpe_gs_t;

extern ysw_lv_hpe_gs_t ysw_lv_hpe_gs;

lv_obj_t *ysw_lv_hpe_create(lv_obj_t *par, void *context);
void ysw_lv_hpe_set_create_cb(lv_obj_t *hpe, void *cb);
void ysw_lv_hpe_set_edit_cb(lv_obj_t *hpe, void *cb);
void ysw_lv_hpe_set_select_cb(lv_obj_t *hpe, void *cb);
void ysw_lv_hpe_set_deselect_cb(lv_obj_t *hpe, void *cb);
void ysw_lv_hpe_set_drag_end_cb(lv_obj_t *hpe, void *cb);
void ysw_lv_hpe_set_hp(lv_obj_t *hpe, ysw_hp_t *hp);
void ysw_lv_hpe_on_metro(lv_obj_t *hpe, ysw_note_t *metro_note);
void ysw_lv_hpe_ensure_visible(lv_obj_t *hpe, uint32_t first_ps_index, uint32_t last_ps_index);

