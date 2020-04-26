// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "stdio.h"
#include "esp_log.h"
#include "lvgl.h"
#include "ysw_chord.h"
#include "ysw_ticks.h"
#include "ysw_lv_cne.h"
#include "ysw_lv_styles.h"

#define TAG "YSW_LV_CNE"

#define LV_OBJX_NAME "ysw_lv_cne"

static lv_design_cb_t super_design_cb;
static lv_signal_cb_t super_signal_cb;

// Note that rows are drawn top down in reverse logical order

static char *key_labels[] =
{
    "7th",
    NULL,
    "5th",
    NULL,
    "3rd",
    NULL,
    "1st",
};

#define ROW_COUNT YSW_MIDI_UNPO
#define COLUMN_COUNT 4

static void draw_main(lv_obj_t *cne, const lv_area_t *mask, lv_design_mode_t mode)
{
    ESP_LOGD(TAG, "draw_main mask.x1=%d, y1=%d, x2=%d, y2=%d", mask->x1, mask->y1, mask->x2, mask->y2);
    super_design_cb(cne, mask, mode);

    ysw_lv_cne_ext_t *ext = lv_obj_get_ext_attr(cne);

    if (!ext->chord) {
        return;
    }

    lv_coord_t h = lv_obj_get_height(cne);
    lv_coord_t w = lv_obj_get_width(cne);

    lv_coord_t x = cne->coords.x1;
    lv_coord_t y = cne->coords.y1;

    lv_coord_t row_h = h / ROW_COUNT; // this is an approximate, due to rounding

    ESP_LOGD(TAG, "draw_main h=%d, w=%d, x=%d, y=%d", h, w, x, y);

    for (lv_coord_t i = 0; i < ROW_COUNT; i++) {

        lv_area_t row_area = {
            .x1 = x,
            .y1 = y + ((i * h) / ROW_COUNT),
            .x2 = x + w,
            .y2 = y + (((i + 1) * h) / ROW_COUNT)
        };

        lv_area_t row_mask;
        if (lv_area_intersect(&row_mask, mask, &row_area)) {


            if (i & 0x01) {
                lv_draw_rect(&row_area, &row_mask, ext->style_oi, ext->style_oi->body.opa);
            } else {
                lv_draw_rect(&row_area, &row_mask, ext->style_ei, ext->style_oi->body.opa);

                lv_area_t label_mask;
                lv_area_t label_area = {
                    .x1 = row_area.x1,
                    .y1 = row_area.y1,
                    .x2 = row_area.x1 + 50,
                    .y2 = row_area.y1 + 15
                };
                if (lv_area_intersect(&label_mask, mask, &label_area)) {
                    lv_draw_label(&label_area,
                            &label_mask,
                            ext->style_ei,
                            ext->style_ei->text.opa,
                            key_labels[i],
                            LV_TXT_FLAG_EXPAND,
                            NULL,
                            NULL,
                            NULL,
                            LV_BIDI_DIR_LTR);
                }
            }

            if (i) {
                lv_point_t point1 = {
                    .x = row_area.x1,
                    .y = row_area.y1
                };
                lv_point_t point2 = {
                    .x = row_area.x1 + w,
                    .y = row_area.y1
                };
                if (i & 0x01) {
                    lv_draw_line(&point1, &point2, mask, ext->style_oi, ext->style_oi->body.border.opa);
                } else {
                    lv_draw_line(&point1, &point2, mask, ext->style_ei, ext->style_ei->body.border.opa);
                }
            }
        }
    }

    for (int i = 0; i < COLUMN_COUNT; i++) {
        lv_point_t point1 = {
            .x = x + ((i * w) / COLUMN_COUNT),
            .y = y
        };
        lv_point_t point2 = {
            .x = x + ((i * w) / COLUMN_COUNT),
            .y = y + h
        };
        lv_draw_line(&point1, &point2, mask, ext->style_ei, ext->style_ei->body.border.opa);
    }

    uint32_t note_count = ysw_chord_get_note_count(ext->chord);

    for (int i = 0; i < note_count; i++) {

        ysw_chord_note_t *note = ysw_chord_get_chord_note(ext->chord, i);

        lv_coord_t x1 = x + (note->start * w) / (4 * YSW_TICKS_DEFAULT_TPB);
        lv_coord_t x2 = x + ((note->start + note->duration) * w) / (4 * YSW_TICKS_DEFAULT_TPB);

        uint8_t remainder = 0;
        uint8_t octave = 0;
        ysw_degree_normalize(note->degree, &remainder, &octave);
        uint8_t degree = remainder + 1;

        lv_coord_t delta_y;
        if (note->accidental == YSW_ACCIDENTAL_FLAT) {
            delta_y = -row_h / 2;
        } else if (note->accidental == YSW_ACCIDENTAL_SHARP) {
            delta_y = +row_h / 2;
        } else {
            delta_y = 0;
        }

        lv_coord_t y1 = y + (((7 - degree) * h) / ROW_COUNT) + delta_y;
        lv_coord_t y2 = y + ((((7 - degree) + 1) * h) / ROW_COUNT) + delta_y;

        ESP_LOGD(TAG, "i=%d, remainder=%d, x1=%d, y1=%d, x2=%d, y2=%d", i, remainder, x1, y1, x2, y2);

        lv_area_t note_area = {
            .x1 = x1,
            .y1 = y1 + 4,
            .x2 = x2,
            .y2 = y2 - 4
        };

        lv_area_t note_mask;

        if (lv_area_intersect(&note_mask, mask, &note_area)) {

            lv_draw_rect(&note_area, &note_mask, ext->style_cn, 128);

            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%d", note->velocity);

            // vertically center the text
            lv_point_t offset = {
                .x = 0,
                .y = ((note_area.y2 - note_area.y1) - ext->style_cn->text.font->line_height) / 2
            };

            lv_draw_label(&note_area,
                    &note_mask,
                    ext->style_cn,
                    ext->style_cn->text.opa,
                    buffer,
                    LV_TXT_FLAG_EXPAND | LV_TXT_FLAG_CENTER,
                    &offset,
                    NULL,
                    NULL,
                    LV_BIDI_DIR_LTR);
        }
    }

}

static bool design_cb(lv_obj_t *cne, const lv_area_t *mask, lv_design_mode_t mode)
{
    bool result = true;
    switch (mode) {
        case LV_DESIGN_COVER_CHK:
            result = super_design_cb(cne, mask, mode);
            break;
        case LV_DESIGN_DRAW_MAIN:
            draw_main(cne, mask, mode);
            break;
        case LV_DESIGN_DRAW_POST:
            break;
    }
    return result;
}

static lv_res_t signal_cb(lv_obj_t *cne, lv_signal_t signal, void *param)
{
    lv_res_t res = super_signal_cb(cne, signal, param);
    if (res == LV_RES_OK) {
        switch (signal) {
            case LV_SIGNAL_GET_TYPE:
                res = lv_obj_handle_get_type_signal(param, LV_OBJX_NAME);
                break;
        }
    }
    return res;
}

lv_obj_t *ysw_lv_cne_create(lv_obj_t *par)
{
    lv_obj_t *cne = lv_obj_create(par, NULL);
    LV_ASSERT_MEM(cne);
    if (cne == NULL) {
        return NULL;
    }

    if (super_signal_cb == NULL) {
        super_signal_cb = lv_obj_get_signal_cb(cne);
    }

    if (super_design_cb == NULL) {
        super_design_cb = lv_obj_get_design_cb(cne);
    }

    ysw_lv_cne_ext_t *ext = lv_obj_allocate_ext_attr(cne, sizeof(ysw_lv_cne_ext_t));
    LV_ASSERT_MEM(ext);
    if (ext == NULL) {
        return NULL;
    }

    ext->chord = NULL;
    ext->style_bg = &lv_style_plain;
    ext->style_oi = &odd_interval_style;
    ext->style_ei = &even_interval_style;
    ext->style_cn = &chord_note_style;

    lv_obj_set_signal_cb(cne, signal_cb);
    lv_obj_set_design_cb(cne, design_cb);

    lv_obj_set_click(cne, false);
    lv_obj_set_size(cne, LV_DPI * 2, LV_DPI / 3);

    lv_obj_set_style(cne, &lv_style_plain);

    return cne;
}

void ysw_lv_cne_set_chord(lv_obj_t *cne, ysw_chord_t *chord)
{
    LV_ASSERT_OBJ(cne, LV_OBJX_NAME);
    ysw_lv_cne_ext_t *ext = lv_obj_get_ext_attr(cne);
    if (ext->chord == chord) {
        return;
    }
    ext->chord = chord;
    lv_obj_invalidate(cne);
}

