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
#include "ysw_cp.h"

typedef enum {
    YSW_LV_CPE_SELECT,
    YSW_LV_CPE_DESELECT,
    YSW_LV_CPE_DRAG_END,
    YSW_LV_CPE_CREATE,
} ysw_lv_cpe_event_t;

typedef struct {
    int8_t degree;
} ysw_lv_cpe_create_t;

typedef struct {
    union {
        ysw_lv_cpe_create_t create;
    };
} ysw_lv_cpe_event_cb_data_t;

typedef void (*ysw_lv_cpe_event_cb_t)(lv_obj_t *ysw_lv_cpe, ysw_lv_cpe_event_t event, ysw_lv_cpe_event_cb_data_t *data);

typedef struct {
    ysw_cp_t *cp;
    ysw_lv_cpe_event_cb_t event_cb;
} ysw_lv_cpe_ext_t;

lv_obj_t *ysw_lv_cpe_create(lv_obj_t *par);
void ysw_lv_cpe_set_event_cb(lv_obj_t *cpe, ysw_lv_cpe_event_cb_t event_cb);
void ysw_lv_cpe_set_cp(lv_obj_t *cpe, ysw_cp_t *cp);

