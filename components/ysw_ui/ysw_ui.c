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

void ysw_ui_get_obj_type(lv_obj_t *obj, char *buffer, uint32_t size)
{
    lv_obj_type_t types;
    lv_obj_get_type(obj, &types);

    char *t = buffer;
    char *t_max = buffer + size - 1; // -1 for null terminator

    uint8_t i;
    bool done = false;
    for (i = 0; i < LV_MAX_ANCESTOR_NUM && !done; i++) {
        if (!types.type[i]) {
            done = true;
        } else {
            if (t >= t_max) {
                done = true;
            } else {
                *t++ = '/';
            }
            const char *s = types.type[i];
            while (*s && !done) {
                if (t >= t_max) {
                    done = true;
                } else {
                    *t++ = *s++;
                }
            }
        }
    }
    *t = 0;
}

int32_t ysw_ui_get_index_of_child(lv_obj_t *obj)
{
    lv_obj_t *parent = lv_obj_get_parent(obj);
    if (parent) {
        int32_t index = 0;
        lv_obj_t *child;
        _LV_LL_READ_BACK(parent->child_ll, child)
        {
            if (child == obj) {
                return index;
            }
            index++;
        }
    }
    return -1;
}

lv_obj_t* ysw_ui_child_at_index(lv_obj_t *parent, uint32_t index)
{
    uint32_t current = 0;
    lv_obj_t *child;
    _LV_LL_READ_BACK(parent->child_ll, child)
    {
        if (current == index) {
            return child;
        }
        current++;
    }
    return NULL;
}

void ysw_ui_ensure_visible(lv_obj_t *child, bool do_center)
{
    lv_obj_t *scrl = lv_obj_get_parent(child);
    lv_coord_t scrl_top = lv_obj_get_y(scrl); // always <= 0

    lv_obj_t *viewport = lv_obj_get_parent(scrl);
    lv_coord_t viewport_height = lv_obj_get_height(viewport);

    lv_coord_t child_height = lv_obj_get_height(child);
    lv_coord_t child_top = lv_obj_get_y(child);
    lv_coord_t child_bottom = child_top + child_height;

    lv_coord_t visible_top = -scrl_top;
    lv_coord_t visible_bottom = visible_top + viewport_height;

    bool is_above = child_top < visible_top;
    bool is_below = child_bottom > visible_bottom;

    if (is_above || is_below) {
        if (do_center || (is_above && is_below)) {
            lv_coord_t center_offset = (viewport_height - child_height) / 2;
            scrl_top = -child_top + center_offset;
        } else if (is_above) {
            scrl_top = -child_top;
        } else if (is_below) {
            lv_coord_t bottom_offset = (viewport_height - child_height);
            scrl_top = -child_top + bottom_offset;
        }
        lv_obj_set_y(scrl, scrl_top);
    }
}


