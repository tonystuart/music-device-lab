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

extern lv_draw_rect_dsc_t rect_dsc;
extern lv_draw_label_dsc_t label_dsc;
extern lv_draw_line_dsc_t line_dsc;

extern lv_draw_line_dsc_t odd_line_dsc;
extern lv_draw_line_dsc_t even_line_dsc;
extern lv_draw_line_dsc_t div_line_dsc;
extern lv_draw_line_dsc_t metro_line_dsc;

extern lv_draw_rect_dsc_t odd_rect_dsc;
extern lv_draw_rect_dsc_t even_rect_dsc;
extern lv_draw_rect_dsc_t sn_rect_dsc;
extern lv_draw_rect_dsc_t sel_sn_rect_dsc;
extern lv_draw_rect_dsc_t drag_sn_rect_dsc;

extern lv_draw_label_dsc_t degree_label_dsc;
extern lv_draw_label_dsc_t sn_label_dsc;
extern lv_draw_label_dsc_t sel_sn_label_dsc;
extern lv_draw_label_dsc_t drag_sn_label_dsc;

void ysw_style_initialize();
