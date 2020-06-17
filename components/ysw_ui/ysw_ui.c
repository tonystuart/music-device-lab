// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "lvgl.h"
#include "esp_log.h"

#define TAG "YSW_UI"


void ysw_ui_distribute_extra_width(lv_obj_t *parent, lv_obj_t *obj)
{
    lv_coord_t inner = lv_obj_get_style_pad_inner(parent, LV_CONT_PART_MAIN);

    lv_obj_t *child;
    lv_coord_t width = lv_obj_get_style_pad_left(parent, LV_CONT_PART_MAIN);
    ;
    _LV_LL_READ_BACK(parent->child_ll, child)
    {
        lv_style_int_t mleft = lv_obj_get_style_margin_left(child, LV_OBJ_PART_MAIN);
        lv_style_int_t mright = lv_obj_get_style_margin_right(child, LV_OBJ_PART_MAIN);
        width += lv_obj_get_width(child) + inner + mleft + mright;
    }
    ESP_LOGD(TAG, "extra_width=%d", width);
    lv_coord_t parent_width = lv_obj_get_width(parent);
    lv_coord_t extra_width = parent_width - width; // can be negative
    lv_obj_set_width(obj, lv_obj_get_width(obj) + extra_width);
}

void ysw_ui_distribute_extra_height(lv_obj_t *parent, lv_obj_t *obj)
{
    lv_coord_t inner = lv_obj_get_style_pad_inner(parent, LV_CONT_PART_MAIN);

    lv_obj_t *child;
    lv_coord_t height = lv_obj_get_style_pad_top(parent, LV_CONT_PART_MAIN);
    ;
    _LV_LL_READ_BACK(parent->child_ll, child)
    {
        lv_style_int_t mtop = lv_obj_get_style_margin_top(child, LV_OBJ_PART_MAIN);
        lv_style_int_t mbottom = lv_obj_get_style_margin_bottom(child, LV_OBJ_PART_MAIN);
        height += lv_obj_get_height(child) + inner + mtop + mbottom;
    }
    ESP_LOGD(TAG, "extra_height=%d", height);
    lv_coord_t parent_height = lv_obj_get_height(parent);
    lv_coord_t extra_height = parent_height - height; // can be negative
    lv_obj_set_height(obj, lv_obj_get_height(obj) + extra_height);
}

void ysw_ui_lighten_background(lv_obj_t *obj)
{
    lv_obj_t *parent = lv_obj_get_parent(obj);
    if (parent) {
        lv_color_t parent_bg = lv_obj_get_style_bg_color(parent, LV_OBJ_PART_MAIN);
        lv_color_t child_bg = lv_color_lighten(parent_bg, LV_OPA_20);
        lv_obj_set_style_local_bg_color(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, child_bg);
    }
}

void ysw_ui_clear_border(lv_obj_t *obj)
{
    lv_obj_set_style_local_border_width(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
}

