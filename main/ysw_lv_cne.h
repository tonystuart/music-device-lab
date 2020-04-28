// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_array.h"

typedef struct
{
    ysw_cs_t *cs;
    const lv_style_t *style_bg; // background
    const lv_style_t *style_oi; // odd interval
    const lv_style_t *style_ei; // even interval
    const lv_style_t *style_cn; // chord note
    const lv_style_t *style_sn; // selected note
} ysw_lv_cse_ext_t;

lv_obj_t *ysw_lv_cse_create(lv_obj_t *par);
void ysw_lv_cse_set_cs(lv_obj_t *cse, ysw_cs_t *cs);
void ysw_lv_cse_select(lv_obj_t *cse, ysw_csn_t *csn, bool is_selected);
bool ysw_lv_cse_is_selected(lv_obj_t *cse, ysw_csn_t *csn);
