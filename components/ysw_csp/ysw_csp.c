// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_csp.h"

#include "ysw_cs.h"
#include "ysw_music.h"

#include "lvgl.h"
#include "lv_debug.h"

#include "esp_log.h"
#include "assert.h"

#define LV_OBJX_NAME "YSW_CSP"
#define TAG LV_OBJX_NAME

static lv_design_cb_t ancestor_design;
static lv_signal_cb_t ancestor_signal;

typedef struct {
    ysw_cs_t *cs;
} ysw_csp_ext_t;

static lv_design_res_t draw_main(lv_obj_t *csp, const lv_area_t *clip_area)
{
    ysw_csp_ext_t *ext = lv_obj_get_ext_attr(csp);
    if (ext->cs) {
        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);

        // The draw_dsc is generally initialized using the the part and the theme:
        //lv_obj_init_draw_rect_dsc(csp, YSW_CSP_PART_NOTE, &rect_dsc);
        rect_dsc.border_width = 0;
        rect_dsc.bg_color = LV_COLOR_MAKE(0xbb, 0x86, 0xfc);
        //rect_dsc.bg_color = LV_COLOR_PURPLE;
        //rect_dsc.bg_color = LV_COLOR_YELLOW;

        lv_coord_t w = lv_area_get_width(&csp->coords);
        lv_coord_t h = lv_area_get_height(&csp->coords);

        lv_style_int_t pad_left = lv_obj_get_style_pad_left(csp, LV_OBJ_PART_MAIN);
        lv_style_int_t pad_right = lv_obj_get_style_pad_right(csp, LV_OBJ_PART_MAIN);
        w -= pad_left + pad_right;
        
        lv_style_int_t pad_top = lv_obj_get_style_pad_top(csp, LV_OBJ_PART_MAIN);
        lv_style_int_t pad_bottom = lv_obj_get_style_pad_bottom(csp, LV_OBJ_PART_MAIN);
        h -= pad_top + pad_bottom;

        float pixels_per_tick = (float)w / YSW_CS_DURATION;
        float pixels_per_degree = (float)h / YSW_MIDI_UNPO;
        uint32_t sn_count = ysw_cs_get_sn_count(ext->cs);
        for (uint32_t i = 0; i < sn_count; i++) {
            ysw_sn_t *sn = ysw_cs_get_sn(ext->cs, i);
            lv_coord_t sn_left = (pixels_per_tick * sn->start) + 1; // +1 for pad
            lv_coord_t sn_right = (pixels_per_tick * (sn->start + sn->duration)) - 1; // -1 for pad
            lv_coord_t sn_top = (pixels_per_degree * (YSW_MIDI_UNPO - sn->degree)) + 1; // +1 for pad
            lv_coord_t sn_bottom = sn_top + pixels_per_degree;
            lv_area_t sn_area = {
                    .x1 = csp->coords.x1 + pad_left + sn_left,
                    .x2 = csp->coords.x1 + pad_left + sn_right,
                    .y1 = csp->coords.y1 + pad_top + sn_top,
                    .y2 = csp->coords.y1 + pad_top + sn_bottom
            };
            lv_draw_rect(&sn_area, clip_area, &rect_dsc);
        }
    }
    return LV_DESIGN_RES_OK;
}

static lv_design_res_t ysw_csp_design(lv_obj_t *csp, const lv_area_t *clip_area, lv_design_mode_t mode)
{
    lv_design_res_t res = ancestor_design(csp, clip_area, mode);
    if (mode == LV_DESIGN_DRAW_MAIN) {
        if (res == LV_DESIGN_RES_OK) {
            res = draw_main(csp, clip_area);
        }
    }
    return res;
}

static lv_res_t ysw_csp_signal(lv_obj_t *csp, lv_signal_t sign, void *param)
{
    lv_res_t res;

    res = ancestor_signal(csp, sign, param);
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

lv_obj_t* ysw_csp_create(lv_obj_t *par)
{
    lv_obj_t *csp = lv_obj_create(par, NULL);
    assert(csp);

    if (!ancestor_signal) {
        ancestor_signal = lv_obj_get_signal_cb(csp);
    }

    if (!ancestor_design) {
        ancestor_design = lv_obj_get_design_cb(csp);
    }

    ysw_csp_ext_t *ext = lv_obj_allocate_ext_attr(csp, sizeof(ysw_csp_ext_t));
    assert(ext);

    ext->cs = NULL;

    lv_obj_set_signal_cb(csp, ysw_csp_signal);
    lv_obj_set_design_cb(csp, ysw_csp_design);

    lv_obj_set_size(csp, 60, 30);
    lv_theme_apply(csp, LV_THEME_OBJ);

    return csp;
}

void ysw_csp_set_cs(lv_obj_t *csp, ysw_cs_t *cs)
{
    ysw_csp_ext_t *ext = lv_obj_get_ext_attr(csp);
    ext->cs = cs;
}
