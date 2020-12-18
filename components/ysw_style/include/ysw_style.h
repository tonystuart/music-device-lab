// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_edit_pane.h"
#include "lvgl/lvgl.h"

#define NORMAL_CELL LV_TABLE_PART_CELL1
#define HEADING_CELL LV_TABLE_PART_CELL2
#define SELECTED_CELL LV_TABLE_PART_CELL3

#define YSW_STYLE_COLOR(c) LV_COLOR_MAKE(((c >> 16) & 0xff), ((c >> 8) & 0xff), (c & 0xff))

void ysw_style_clear_border(lv_obj_t *obj);
lv_color_t ysw_style_get_child_background(lv_obj_t *obj);
lv_color_t ysw_style_lighten_background(lv_obj_t *obj);
lv_color_t ysw_style_adjust_obj(lv_obj_t *obj);
void ysw_style_adjust_field_name(lv_obj_t *label);
void ysw_style_adjust_keyboard(lv_obj_t *kb);
void ysw_style_adjust_ddlist(lv_obj_t *ddlist);
void ysw_style_adjust_btn(lv_obj_t *btn);
void ysw_style_adjust_button_bar(lv_obj_t *btn);
void ysw_style_adjust_checkbox(lv_obj_t *cb);
void ysw_style_adjust_mbox(lv_obj_t *mbox);

void ysw_style_editor(lv_obj_t *container);
void ysw_style_chooser(lv_obj_t *page, lv_obj_t *table);
void ysw_style_softkeys(lv_obj_t *container, lv_obj_t *label, lv_obj_t *btnmatrix);
void ysw_style_popup(lv_obj_t *container, lv_obj_t *msgbox);
void ysw_style_field(lv_obj_t *name_label, lv_obj_t *field);
void ysw_style_edit_pane(lv_obj_t *container, lv_obj_t *textarea, lv_obj_t *keyboard);
