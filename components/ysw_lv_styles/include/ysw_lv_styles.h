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


extern lv_style_t plain_color_tight;
extern lv_style_t even_interval_style;
extern lv_style_t odd_interval_style;
extern lv_style_t cn_style;
extern lv_style_t selected_cn_style;

extern lv_style_t cell_editor_style;
extern lv_style_t cell_selection_style;

extern lv_style_t value_cell;
extern lv_style_t win_style_content;
extern lv_style_t page_bg_style;
extern lv_style_t page_scrl_style;
extern lv_style_t name_cell;
extern lv_style_t selected_cell;

void ysw_lv_styles_initialize();
