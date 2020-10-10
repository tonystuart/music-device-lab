// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_staff.h"
#include "ysw_array.h"
#include "ysw_note.h"
#include "lvgl.h"
#include "esp_log.h"
#include "assert.h"

#define LV_OBJX_NAME "ysw_staff"

#define LED_WIDTH_DEF (LV_DPI / 3)
#define LED_HEIGHT_DEF (LV_DPI / 3)

static lv_design_cb_t ancestor_design;
static lv_signal_cb_t ancestor_signal;

#define TAG "STAFF"

static lv_design_res_t ysw_staff_design(lv_obj_t *staff, const lv_area_t *clip_area, lv_design_mode_t mode)
{
    if (mode == LV_DESIGN_COVER_CHK) {
        return ancestor_design(staff, clip_area, mode);
    } else if (mode == LV_DESIGN_DRAW_MAIN) {
        ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);

        if (ext->notes) {
            lv_draw_rect_dsc_t rect_dsc;
            lv_draw_rect_dsc_init(&rect_dsc);
            lv_obj_init_draw_rect_dsc(staff, YSW_STAFF_PART_MAIN, &rect_dsc);

            rect_dsc.bg_color = LV_COLOR_RED;
            rect_dsc.bg_grad_color = LV_COLOR_BLUE;
            rect_dsc.border_color = LV_COLOR_PURPLE;
            rect_dsc.shadow_color = LV_COLOR_GRAY;

            rect_dsc.shadow_width = 0;
            rect_dsc.shadow_spread = 0;

            uint16_t note_count = ysw_array_get_count(ext->notes);
            for (uint16_t i = 0; i < note_count; i++) {
                ysw_note_t *note = ysw_array_get(ext->notes, i);
                lv_area_t coords = {
                    .x1 = note->start / 8,
                    .y1 = note->midi_note * 2 - 5,
                    .x2 = note->start / 8 + 10,
                    .y2 = note->midi_note * 2 + 5
                };
                lv_draw_rect(&coords, clip_area, &rect_dsc);
            }
        }
    }
    return LV_DESIGN_RES_OK;
}

static lv_res_t ysw_staff_signal(lv_obj_t *staff, lv_signal_t sign, void *param)
{
    lv_res_t res;

    res = ancestor_signal(staff, sign, param);
    if (res != LV_RES_OK) return res;

    if (sign == LV_SIGNAL_GET_TYPE) {
        lv_obj_type_t *buf = param;
        uint8_t i;
        for(i = 0; i < LV_MAX_ANCESTOR_NUM - 1; i++) {
            if (buf->type[i] == NULL) break;
        }
        buf->type[i] = "ysw_staff";
    }

    return res;
}

lv_obj_t *ysw_staff_create(lv_obj_t *par, const lv_obj_t *copy)
{
    lv_obj_t *staff = lv_obj_create(par, copy);
    if (staff == NULL) {
        return NULL;
    }

    if (ancestor_signal == NULL) {
        ancestor_signal = lv_obj_get_signal_cb(staff);
    }

    if (ancestor_design == NULL) {
        ancestor_design = lv_obj_get_design_cb(staff);
    }

    ysw_staff_ext_t *ext = lv_obj_allocate_ext_attr(staff, sizeof(ysw_staff_ext_t));
    if (ext == NULL) {
        lv_obj_del(staff);
        return NULL;
    }

    lv_obj_set_signal_cb(staff, ysw_staff_signal);
    lv_obj_set_design_cb(staff, ysw_staff_design);

    if (copy == NULL) {
        lv_obj_set_size(staff, LED_WIDTH_DEF, LED_HEIGHT_DEF);
        lv_theme_apply(staff, LV_THEME_LED);
    } else {
        ysw_staff_ext_t *copy_ext = lv_obj_get_ext_attr(copy);
        lv_obj_refresh_style(staff, LV_OBJ_PART_ALL, LV_STYLE_PROP_ALL);
    }

    return staff;
}

void ysw_staff_set_notes(lv_obj_t *staff, ysw_array_t *notes)
{
    assert(staff);

    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    ext->notes = notes;
    lv_obj_invalidate(staff);
}

ysw_array_t *ysw_staff_get_notes(const lv_obj_t *staff)
{
    assert(staff);

    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    return ext->notes;
}

