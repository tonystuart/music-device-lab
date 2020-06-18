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
#include "stdint.h"

void ysw_ui_distribute_extra_width(lv_obj_t *parent, lv_obj_t *obj);
void ysw_ui_distribute_extra_height(lv_obj_t *parent, lv_obj_t *obj);
void ysw_ui_lighten_background(lv_obj_t *obj);
void ysw_ui_clear_border(lv_obj_t *obj);
void ysw_ui_get_obj_type(lv_obj_t *obj, char *buffer, uint32_t size);
int32_t ysw_ui_get_index_of_child(lv_obj_t *obj);
lv_obj_t* ysw_ui_child_at_index(lv_obj_t *parent, uint32_t index);
