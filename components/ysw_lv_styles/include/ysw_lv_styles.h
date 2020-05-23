// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "lvgl/lvgl.h"

extern lv_style_t ysw_style_plain_color_tight;
extern lv_style_t ysw_style_pretty_color_tight;
extern lv_style_t ysw_style_sdb_content;

extern lv_style_t ysw_style_ei; // cse even interval
extern lv_style_t ysw_style_oi; // cse odd interval
extern lv_style_t ysw_style_rn; // cse regular note
extern lv_style_t ysw_style_sn; // cse selected note
extern lv_style_t ysw_style_mn; // cse metro note

extern lv_style_t ysw_style_gray_cell;
extern lv_style_t ysw_style_white_cell;
extern lv_style_t ysw_style_yellow_cell;
extern lv_style_t ysw_style_red_cell;


void ysw_lv_styles_initialize();
