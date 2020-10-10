// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_hpp.h"

#include "ysw_hp.h"
#include "ysw_hpe.h"
#include "ysw_music.h"
#include "ysw_style.h"

#include "lvgl.h"

#include "esp_log.h"
#include "assert.h"

#define LV_OBJX_NAME "YSW_HPP"
#define TAG LV_OBJX_NAME

static lv_design_cb_t base_design;
static lv_signal_cb_t base_signal;

typedef struct {
    ysw_hp_t *hp;
} ysw_hpp_ext_t;

static lv_design_res_t draw_main(lv_obj_t *hpp, const lv_area_t *clip_area)
{
    ysw_hpp_ext_t *ext = lv_obj_get_ext_attr(hpp);
    if (ext->hp) {
        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);

        lv_coord_t w = lv_area_get_width(&hpp->coords);
        lv_coord_t h = lv_area_get_height(&hpp->coords);

        lv_style_int_t pad_left = lv_obj_get_style_pad_left(hpp, LV_OBJ_PART_MAIN);
        lv_style_int_t pad_right = lv_obj_get_style_pad_right(hpp, LV_OBJ_PART_MAIN);
        w -= pad_left + pad_right;
        
        lv_style_int_t pad_top = lv_obj_get_style_pad_top(hpp, LV_OBJ_PART_MAIN);
        lv_style_int_t pad_bottom = lv_obj_get_style_pad_bottom(hpp, LV_OBJ_PART_MAIN);
        h -= pad_top + pad_bottom;

        float pixels_per_step = (float)w / YSW_HPE_MAX_COLS;
        float pixels_per_degree = (float)h / YSW_MIDI_UNPO;
        uint32_t ps_count = min(ysw_hp_get_ps_count(ext->hp), YSW_HPE_MAX_COLS);
        for (uint32_t i = 0; i < ps_count; i++) {
            ysw_ps_t *ps = ysw_hp_get_ps(ext->hp, i);
            uint8_t row = YSW_MIDI_UNPO - ps->degree;
            lv_coord_t ps_left = (pixels_per_step * i) + 1; // +1 for pad
            lv_coord_t ps_right = (pixels_per_step * (i + 1)) - 1; // -1 for pad
            lv_coord_t ps_top = (pixels_per_degree * row) + 1; // +1 for pad
            lv_coord_t ps_bottom = ps_top + pixels_per_degree - 1; // -1 for pad
            lv_area_t ps_area = {
                    .x1 = hpp->coords.x1 + pad_left + ps_left,
                    .x2 = hpp->coords.x1 + pad_left + ps_right,
                    .y1 = hpp->coords.y1 + pad_top + ps_top,
                    .y2 = hpp->coords.y1 + pad_top + ps_bottom
            };
            lv_draw_rect(&ps_area, clip_area, &ysw_style_hpp_rect_dsc);
        }
    }
    return LV_DESIGN_RES_OK;
}

static lv_design_res_t ysw_hpp_design(lv_obj_t *hpp, const lv_area_t *clip_area, lv_design_mode_t mode)
{
    lv_design_res_t res = base_design(hpp, clip_area, mode);
    if (mode == LV_DESIGN_DRAW_MAIN) {
        if (res == LV_DESIGN_RES_OK) {
            res = draw_main(hpp, clip_area);
        }
    }
    return res;
}

static lv_res_t ysw_hpp_signal(lv_obj_t *hpp, lv_signal_t sign, void *param)
{
    lv_res_t res;

    res = base_signal(hpp, sign, param);
    if (res != LV_RES_OK) {
        return res;
    }

    if (sign == LV_SIGNAL_GET_TYPE) {
        lv_obj_type_t *buf = param;
        uint8_t i;
        for (i = 0; i < LV_MAX_ANCESTOR_NUM - 1; i++) { /*Find the last set data*/
            if (buf->type[i] == NULL)
                break;
        }
        buf->type[i] = LV_OBJX_NAME;
    }

    return res;
}

lv_obj_t* ysw_hpp_create(lv_obj_t *par)
{
    lv_obj_t *hpp = lv_obj_create(par, NULL);
    assert(hpp);

    if (!base_signal) {
        base_signal = lv_obj_get_signal_cb(hpp);
    }

    if (!base_design) {
        base_design = lv_obj_get_design_cb(hpp);
    }

    ysw_hpp_ext_t *ext = lv_obj_allocate_ext_attr(hpp, sizeof(ysw_hpp_ext_t));
    assert(ext);

    ext->hp = NULL;

    lv_obj_set_signal_cb(hpp, ysw_hpp_signal);
    lv_obj_set_design_cb(hpp, ysw_hpp_design);

    lv_obj_set_size(hpp, 60, 30);
    lv_theme_apply(hpp, LV_THEME_OBJ);

    return hpp;
}

void ysw_hpp_set_hp(lv_obj_t *hpp, ysw_hp_t *hp)
{
    ysw_hpp_ext_t *ext = lv_obj_get_ext_attr(hpp);
    ext->hp = hp;
}
