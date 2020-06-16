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

#if 0
extern lv_style_t ysw_style_none;
extern lv_style_t ysw_style_pretty_color_tight;
extern lv_style_t ysw_style_sdb_content;

extern lv_style_t ysw_style_ei; // cse even interval
extern lv_style_t ysw_style_oi; // cse odd interval
extern lv_style_t ysw_style_rn; // cse regular note
extern lv_style_t ysw_style_sn; // cse selected note
extern lv_style_t ysw_style_mn; // cse metro note

extern lv_style_t ysw_style_table_bg;
extern lv_style_t ysw_style_white_cell;
extern lv_style_t ysw_style_gray_cell;
extern lv_style_t ysw_style_yellow_cell;

extern lv_style_t ysw_style_btn_rel;
extern lv_style_t ysw_style_btn_pr;

extern lv_style_t ysw_style_red_test;
#else
extern lv_draw_rect_dsc_t rect_dsc;
extern lv_draw_label_dsc_t label_dsc;
extern lv_draw_line_dsc_t line_dsc;
#endif

void ysw_styles_initialize();
