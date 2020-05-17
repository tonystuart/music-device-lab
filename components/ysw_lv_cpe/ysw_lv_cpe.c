// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// Chord Progression Editor (CPE)

#include "ysw_lv_cpe.h"

#include "ysw_cp.h"
#include "ysw_cn.h"
#include "ysw_lv_styles.h"
#include "ysw_ticks.h"

#include "lvgl.h"

#include "esp_log.h"

#include "math.h"
#include "stdio.h"

#define TAG "YSW_LV_CPE"

#define LV_OBJX_NAME "ysw_lv_cpe"

#define ROW_COUNT 7

static lv_design_cb_t super_design_cb;
static lv_signal_cb_t super_signal_cb;

static char *key_labels[] =
{
    "I",
    "II",
    "III",
    "IV",
    "V",
    "VI",
    "VII",
};

static void draw_line(lv_coord_t x1, lv_coord_t y1, lv_coord_t x2, lv_coord_t y2, const lv_area_t *mask, const lv_style_t *style)
{
    lv_point_t p1 = {
        .x = x1,
        .y = y1,
    };
    lv_point_t p2 = {
        .x = x2,
        .y = y2,
    };
    lv_draw_line(&p1, &p2, mask, style, style->body.border.opa);
}

static void draw_main(lv_obj_t *cpe, const lv_area_t *mask, lv_design_mode_t mode)
{
    super_design_cb(cpe, mask, mode);

    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);

    if (!ext->cp) {
        return;
    }

    uint32_t step_count = ysw_cp_get_step_count(ext->cp);

    lv_coord_t h = lv_obj_get_height(cpe);
    lv_coord_t w = lv_obj_get_width(cpe);

    lv_coord_t x = cpe->coords.x1;
    lv_coord_t y = cpe->coords.y1;

    lv_coord_t column_count = step_count > 16 ? 16 : step_count > 0 ? step_count : 1;
    lv_coord_t column_width = w / column_count;
    lv_coord_t row_height = h / 9; // 9 = header + 7 degrees + footer

    lv_coord_t px = x;
    lv_coord_t py = y + row_height;

    lv_coord_t pw = w;
    lv_coord_t ph = h - (2 * row_height);

    ESP_LOGD(TAG, "h=%d, w=%d, ph=%d, selected=%d", h, w, ph, ext->selected_step);

    draw_line(px, py, px + pw, py, mask, ext->style_ei);
    draw_line(px, py + ph, px + pw, py + ph, mask, ext->style_ei);

    uint32_t measure = 0;

    for (uint32_t i = 0; i < step_count; i++) {
        ysw_step_t *step = ysw_cp_get_step(ext->cp, i);

        lv_coord_t left = px + (i * column_width);

        if (step->flags & YSW_STEP_NEW_MEASURE) {
            measure++;

            lv_area_t measure_heading_area = {
                .x1 = left,
                .y1 = y,
                .x2 = left + column_width,
                .y2 = py,
            };

            lv_area_t measure_heading_mask;

            if (lv_area_intersect(&measure_heading_mask, mask, &measure_heading_area)) {
                lv_point_t offset = {
                    .x = 0,
                    .y = 0, // e.g. to center vertically
                };
                char buffer[32];
                ysw_itoa(measure, buffer, sizeof(buffer));
                lv_draw_label(&measure_heading_area,
                        &measure_heading_mask,
                        ext->style_ei,
                        ext->style_ei->text.opa,
                        buffer,
                        LV_TXT_FLAG_EXPAND | LV_TXT_FLAG_CENTER,
                        &offset,
                        NULL,
                        NULL,
                        LV_BIDI_DIR_LTR);
            }

        }

        if (i) {
            lv_coord_t column_top = step->flags & YSW_STEP_NEW_MEASURE ? y : py;
            draw_line(left, column_top, left, py + ph, mask, ext->style_ei);
        }

        lv_coord_t cell_top = py + ((7 - step->degree) * row_height);

        lv_area_t cell_area = {
            .x1 = left,
            .y1 = cell_top,
            .x2 = left + column_width,
            .y2 = cell_top + row_height,
        };

        lv_area_t cell_mask;

        if (lv_area_intersect(&cell_mask, mask, &cell_area)) {

            if (ext->selected_step == i) {
                lv_draw_rect(&cell_area, &cell_mask, ext->style_sn, ext->style_sn->body.opa);
            } else {
                lv_draw_rect(&cell_area, &cell_mask, ext->style_cn, ext->style_cn->body.opa);
            }

            // vertically center the text
            lv_point_t offset = {
                .x = 0,
                .y = ((cell_area.y2 - cell_area.y1) - ext->style_ei->text.font->line_height) / 2
            };

            lv_draw_label(&cell_area,
                    &cell_mask,
                    ext->style_ei,
                    ext->style_ei->text.opa,
                    key_labels[to_index(step->degree)],
                    LV_TXT_FLAG_EXPAND | LV_TXT_FLAG_CENTER,
                    &offset,
                    NULL,
                    NULL,
                    LV_BIDI_DIR_LTR);
        }

        if (ext->selected_step == i) {
            lv_area_t footer_area = {
                .x1 = x,
                .y1 = py + ph,
                .x2 = x + w,
                .y2 = py + ph + row_height,
            };

            lv_area_t footer_mask;

            if (lv_area_intersect(&footer_mask, mask, &footer_area)) {

                // vertically center the text
                lv_point_t offset = {
                    .x = 0,
                    .y = ((footer_area.y2 - footer_area.y1) - ext->style_ei->text.font->line_height) / 2
                };

                lv_draw_label(&footer_area,
                        &footer_mask,
                        ext->style_ei,
                        ext->style_ei->text.opa,
                        step->cs->name,
                        LV_TXT_FLAG_EXPAND | LV_TXT_FLAG_CENTER,
                        &offset,
                        NULL,
                        NULL,
                        LV_BIDI_DIR_LTR);
            }
        }

    }
}

static bool design_cb(lv_obj_t *cpe, const lv_area_t *mask, lv_design_mode_t mode)
{
    bool result = true;
    switch (mode) {
        case LV_DESIGN_COVER_CHK:
            result = super_design_cb(cpe, mask, mode);
            break;
        case LV_DESIGN_DRAW_MAIN:
            draw_main(cpe, mask, mode);
            break;
        case LV_DESIGN_DRAW_POST:
            break;
    }
    return result;
}

static lv_res_t signal_cb(lv_obj_t *cpe, lv_signal_t signal, void *param)
{
    lv_res_t res = super_signal_cb(cpe, signal, param);
    //ESP_LOGD(TAG, "signal_cb signal=%d", signal);
    if (res == LV_RES_OK) {
        switch (signal) {
            case LV_SIGNAL_GET_TYPE:
                res = lv_obj_handle_get_type_signal(param, LV_OBJX_NAME);
                break;
            case LV_SIGNAL_CLEANUP:
                // free anything we allocated
                // note that lv_obj_del frees ext_attr if set
                break;
            case LV_SIGNAL_PRESSED:
                break;
            case LV_SIGNAL_PRESSING:
                break;
            case LV_SIGNAL_RELEASED:
                break;
            case LV_SIGNAL_PRESS_LOST:
                break;
            case LV_SIGNAL_LONG_PRESS:
                break;
        }
    }
    return res;
}

lv_obj_t *ysw_lv_cpe_create(lv_obj_t *par)
{
    lv_obj_t *cpe = lv_obj_create(par, NULL);
    LV_ASSERT_MEM(cpe);
    if (cpe == NULL) {
        return NULL;
    }

    if (super_signal_cb == NULL) {
        super_signal_cb = lv_obj_get_signal_cb(cpe);
    }

    if (super_design_cb == NULL) {
        super_design_cb = lv_obj_get_design_cb(cpe);
    }

    ysw_lv_cpe_ext_t *ext = lv_obj_allocate_ext_attr(cpe, sizeof(ysw_lv_cpe_ext_t));
    LV_ASSERT_MEM(ext);
    if (ext == NULL) {
        return NULL;
    }

    ext->cp = NULL;
    ext->selected_step = 6;

    ext->style_bg = &lv_style_plain;
    ext->style_oi = &odd_interval_style;
    ext->style_ei = &even_interval_style;
    ext->style_cn = &cn_style;
    ext->style_sn = &selected_cn_style;

    ext->event_cb = NULL;

    lv_obj_set_signal_cb(cpe, signal_cb);
    lv_obj_set_design_cb(cpe, design_cb);
    lv_obj_set_click(cpe, true);
    lv_obj_set_style(cpe, &lv_style_plain);

    return cpe;
}

void ysw_lv_cpe_set_cp(lv_obj_t *cpe, ysw_cp_t *cp)
{
    LV_ASSERT_OBJ(cpe, LV_OBJX_NAME);
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    if (ext->cp == cp) {
        return;
    }
    ext->cp = cp;
    lv_obj_invalidate(cpe);
}

void ysw_lv_cpe_set_event_cb(lv_obj_t *cpe, ysw_lv_cpe_event_cb_t event_cb)
{
    LV_ASSERT_OBJ(cpe, LV_OBJX_NAME);
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    ext->event_cb = event_cb;
}
