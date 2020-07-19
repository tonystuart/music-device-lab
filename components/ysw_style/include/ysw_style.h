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

#define YSW_STYLE_COLOR(c) LV_COLOR_MAKE(((c >> 16) & 0xff), ((c >> 8) & 0xff), (c & 0xff))

extern lv_draw_rect_dsc_t rect_dsc;
extern lv_draw_label_dsc_t label_dsc;
extern lv_draw_line_dsc_t line_dsc;

extern lv_draw_line_dsc_t odd_line_dsc;
extern lv_draw_line_dsc_t even_line_dsc;
extern lv_draw_line_dsc_t div_line_dsc;
extern lv_draw_line_dsc_t metro_line_dsc;

extern lv_draw_rect_dsc_t odd_rect_dsc;
extern lv_draw_rect_dsc_t even_rect_dsc;

// Chord Styles

extern lv_draw_rect_dsc_t sn_rect_dsc;
extern lv_draw_rect_dsc_t sel_sn_rect_dsc;
extern lv_draw_rect_dsc_t drag_sn_rect_dsc;

extern lv_draw_label_dsc_t sn_label_dsc;
extern lv_draw_label_dsc_t sel_sn_label_dsc;
extern lv_draw_label_dsc_t drag_sn_label_dsc;

// Chord Style Profile

extern lv_draw_rect_dsc_t ysw_style_csp_rect_dsc;

// Progressions

extern lv_draw_rect_dsc_t hp_rect_dsc;
extern lv_draw_rect_dsc_t sel_hp_rect_dsc;
extern lv_draw_rect_dsc_t drag_hp_rect_dsc;

extern lv_draw_label_dsc_t hp_label_dsc;
extern lv_draw_label_dsc_t sel_hp_label_dsc;
extern lv_draw_label_dsc_t drag_hp_label_dsc;

// Harmonic Progression Profile

extern lv_draw_rect_dsc_t ysw_style_hpp_rect_dsc;

void ysw_style_initialize();
void ysw_style_clear_border(lv_obj_t *obj);
lv_color_t ysw_style_get_child_background(lv_obj_t *obj);
lv_color_t ysw_style_lighten_background(lv_obj_t *obj);
lv_color_t ysw_style_adjust_obj(lv_obj_t *obj);
void ysw_style_adjust_sdb_container(lv_obj_t *container);
void ysw_style_adjust_field_name(lv_obj_t *label);
void ysw_style_adjust_keyboard(lv_obj_t *kb);
void ysw_style_adjust_ddlist(lv_obj_t *ddlist);
void ysw_style_adjust_btn(lv_obj_t *btn);
void ysw_style_adjust_button_bar(lv_obj_t *btn);
void ysw_style_adjust_checkbox(lv_obj_t *cb);
void ysw_style_adjust_mbox(lv_obj_t *mbox);
void ysw_style_adjust_csl_selection(lv_obj_t *child, bool is_selected);
void ysw_style_adjust_hpl_selection(lv_obj_t *child, bool is_selected);
