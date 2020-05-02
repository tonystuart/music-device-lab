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
#include "ysw_csn.h"
#include "ysw_cs.h"

typedef enum {
    YSW_LV_CSE_SELECT,
    YSW_LV_CSE_DESELECT,
    YSW_LV_CSE_DOUBLE_CLICK
} ysw_lv_cse_event_t;

typedef struct {
    ysw_csn_t *csn;
} ysw_lv_cse_select_t;

typedef struct {
    ysw_csn_t *csn;
} ysw_lv_cse_deselect_t;

typedef struct {
    uint32_t start;
    int8_t degree;
} ysw_lv_cse_double_click_t;

typedef struct {
    union {
        ysw_lv_cse_select_t select;
        ysw_lv_cse_deselect_t deselect;
        ysw_lv_cse_double_click_t double_click;
    };
} ysw_lv_cse_event_cb_data_t;

typedef void (*ysw_lv_cse_event_cb_t)(lv_obj_t *ysw_lv_cse, ysw_lv_cse_event_t event, ysw_lv_cse_event_cb_data_t *data);

typedef struct {
    ysw_cs_t *cs;
    lv_point_t last_click;
    const lv_style_t *style_bg; // background
    const lv_style_t *style_oi; // odd interval
    const lv_style_t *style_ei; // even interval
    const lv_style_t *style_cn; // chord note
    const lv_style_t *style_sn; // selected note
    ysw_lv_cse_event_cb_t event_cb;
} ysw_lv_cse_ext_t;

lv_obj_t *ysw_lv_cse_create(lv_obj_t *par);
void ysw_lv_cse_set_cs(lv_obj_t *cse, ysw_cs_t *cs);
void ysw_lv_cse_select(lv_obj_t *cse, ysw_csn_t *csn, bool is_selected);
bool ysw_lv_cse_is_selected(lv_obj_t *cse, ysw_csn_t *csn);
void ysw_lv_cse_set_event_cb(lv_obj_t *cse, ysw_lv_cse_event_cb_t event_cb);
