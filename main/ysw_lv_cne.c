// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// Chord Style Editor (CSE)

#include "stdio.h"
#include "esp_log.h"
#include "lvgl.h"
#include "ysw_chord.h"
#include "ysw_ticks.h"
#include "ysw_lv_cne.h"
#include "ysw_lv_styles.h"

#define TAG "YSW_LV_CSE"

#define LV_OBJX_NAME "ysw_lv_cse"

// Chord Style Note state fields

#define YSW_CSN_SELECTED 0b00000001

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

static void get_note_info(
        lv_obj_t *cse,
        ysw_csn_t *csn,
        lv_area_t *ret_area,
        uint8_t *ret_degree,
        int8_t *ret_octave)
{
    uint8_t degree;
    int8_t octave;

    ysw_degree_normalize(csn->degree, &degree, &octave);

    lv_coord_t h = lv_obj_get_height(cse);
    lv_coord_t w = lv_obj_get_width(cse);

    lv_coord_t x = cse->coords.x1;
    lv_coord_t y = cse->coords.y1;

    lv_coord_t delta_y = -ysw_csn_get_accidental(csn) * ((h / ROW_COUNT) / 2);

    lv_coord_t x1 = x + (csn->start * w) / (4 * YSW_TICKS_DEFAULT_TPB);
    lv_coord_t x2 = x + ((csn->start + csn->duration) * w) / (4 * YSW_TICKS_DEFAULT_TPB);

    lv_coord_t y1 = y + (((YSW_MIDI_UNPO - degree) * h) / ROW_COUNT) + delta_y;
    lv_coord_t y2 = y + ((((YSW_MIDI_UNPO - degree) + 1) * h) / ROW_COUNT) + delta_y;

    lv_area_t area = {
        .x1 = x1,
        .y1 = y1 + 4,
        .x2 = x2,
        .y2 = y2 - 4
    };

    if (ret_area) {
        *ret_area = area;
    }

    if (ret_degree) {
        *ret_degree = degree;
    }

    if (ret_octave) {
        *ret_octave = octave;
    }
}

static void draw_main(lv_obj_t *cse, const lv_area_t *mask, lv_design_mode_t mode)
{
    ESP_LOGD(TAG, "draw_main mask.x1=%d, y1=%d, x2=%d, y2=%d", mask->x1, mask->y1, mask->x2, mask->y2);
    super_design_cb(cse, mask, mode);

    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);

    if (!ext->cs) {
        return;
    }

    lv_coord_t h = lv_obj_get_height(cse);
    lv_coord_t w = lv_obj_get_width(cse);

    lv_coord_t x = cse->coords.x1;
    lv_coord_t y = cse->coords.y1;

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

    uint32_t note_count = ysw_cs_get_note_count(ext->cs);

    for (int i = 0; i < note_count; i++) {

        ysw_csn_t *note = ysw_cs_get_csn(ext->cs, i);

        int8_t octave = 0;
        lv_area_t note_area = {};
        get_note_info(cse, note, &note_area, NULL, &octave);

        lv_area_t note_mask;

        if (lv_area_intersect(&note_mask, mask, &note_area)) {

            if (ysw_lv_cse_is_selected(cse, note)) {
                lv_draw_rect(&note_area, &note_mask, ext->style_sn, 128);
            } else {
                lv_draw_rect(&note_area, &note_mask, ext->style_cn, 128);
            }

            char buffer[32];
            if (octave) {
                snprintf(buffer, sizeof(buffer), "%d/%+d", note->velocity, octave);
            } else {
                snprintf(buffer, sizeof(buffer), "%d", note->velocity);
            }

            // vertically center the text
            lv_point_t offset = {
                .x = 0,
                .y = ((note_area.y2 - note_area.y1) - ext->style_cn->text.font->line_height) / 2
            };

            if (ysw_lv_cse_is_selected(cse, note)) {
                lv_draw_label(&note_area,
                        &note_mask,
                        ext->style_sn,
                        ext->style_sn->text.opa,
                        buffer,
                        LV_TXT_FLAG_EXPAND | LV_TXT_FLAG_CENTER,
                        &offset,
                        NULL,
                        NULL,
                        LV_BIDI_DIR_LTR);
            } else {
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

}

static bool design_cb(lv_obj_t *cse, const lv_area_t *mask, lv_design_mode_t mode)
{
    bool result = true;
    switch (mode) {
        case LV_DESIGN_COVER_CHK:
            result = super_design_cb(cse, mask, mode);
            break;
        case LV_DESIGN_DRAW_MAIN:
            draw_main(cse, mask, mode);
            break;
        case LV_DESIGN_DRAW_POST:
            break;
    }
    return result;
}

#define SELECTION_BORDER 5

static bool in_bounds(lv_area_t *area, lv_coord_t x, lv_coord_t y)
{
    return ((area->x1 - SELECTION_BORDER) <= x && x <= (area->x2 + SELECTION_BORDER)) &&
        ((area->y1 - SELECTION_BORDER) <= y && y <= (area->y2 + SELECTION_BORDER));
}

static void on_pressed(lv_obj_t *cse, void *param)
{
    lv_indev_t *indev_act = (lv_indev_t*)param;
    lv_indev_proc_t *proc = &indev_act->proc;
    lv_coord_t x = proc->types.pointer.act_point.x;
    lv_coord_t y = proc->types.pointer.act_point.y;
    ESP_LOGD(TAG, "on_pressed x=%d, y=%d", x, y);

    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    uint32_t note_count = ysw_cs_get_note_count(ext->cs);
    uint32_t hit_count = 0;

    for (int i = 0; i < note_count; i++) {
        lv_area_t note_area;
        ysw_csn_t *note = ysw_cs_get_csn(ext->cs, i);
        get_note_info(cse, note, &note_area, NULL, NULL);

        if (in_bounds(&note_area, x, y)) {
            bool selected = !ysw_lv_cse_is_selected(cse, note);
            ysw_lv_cse_select(cse, note, selected);
            hit_count++;
        }
    }

    if (!hit_count) {
        for (int i = 0; i < note_count; i++) {
            ysw_csn_t *note = ysw_cs_get_csn(ext->cs, i);
            ysw_lv_cse_select(cse, note, false);
        }
    }
}

static lv_res_t signal_cb(lv_obj_t *cse, lv_signal_t signal, void *param)
{
    ESP_LOGD(TAG, "signal_cb");
    lv_res_t res = super_signal_cb(cse, signal, param);
    if (res == LV_RES_OK) {
        switch (signal) {
            case LV_SIGNAL_GET_TYPE:
                res = lv_obj_handle_get_type_signal(param, LV_OBJX_NAME);
                break;
            case LV_SIGNAL_CLEANUP:
                // free anything we allocated
                break;
            case LV_SIGNAL_PRESSED:
                on_pressed(cse, param);
                break;
        }
    }
    return res;
}

#if 0
static void event_cb(lv_obj_t *obj, lv_event_t event)
{
    switch(event) {
        case LV_EVENT_PRESSED:
            ESP_LOGD(TAG, "Pressed\n");
            break;

        case LV_EVENT_SHORT_CLICKED:
            ESP_LOGD(TAG, "Short clicked\n");
            break;

        case LV_EVENT_CLICKED:
            ESP_LOGD(TAG, "Clicked\n");
            break;

        case LV_EVENT_LONG_PRESSED:
            ESP_LOGD(TAG, "Long press\n");
            break;

        case LV_EVENT_LONG_PRESSED_REPEAT:
            ESP_LOGD(TAG, "Long press repeat\n");
            break;

        case LV_EVENT_RELEASED:
            ESP_LOGD(TAG, "Released\n");
            break;
    }
}
#endif

lv_obj_t *ysw_lv_cse_create(lv_obj_t *par)
{
    lv_obj_t *cse = lv_obj_create(par, NULL);
    LV_ASSERT_MEM(cse);
    if (cse == NULL) {
        return NULL;
    }

    if (super_signal_cb == NULL) {
        super_signal_cb = lv_obj_get_signal_cb(cse);
    }

    if (super_design_cb == NULL) {
        super_design_cb = lv_obj_get_design_cb(cse);
    }

    ysw_lv_cse_ext_t *ext = lv_obj_allocate_ext_attr(cse, sizeof(ysw_lv_cse_ext_t));
    LV_ASSERT_MEM(ext);
    if (ext == NULL) {
        return NULL;
    }

    ext->cs = NULL;
    ext->style_bg = &lv_style_plain;
    ext->style_oi = &odd_interval_style;
    ext->style_ei = &even_interval_style;
    ext->style_cn = &csn_style;
    ext->style_sn = &selected_note_style;

    lv_obj_set_signal_cb(cse, signal_cb);
    lv_obj_set_design_cb(cse, design_cb);

#if 0
    lv_obj_set_event_cb(cse, event_cb);
#endif

    lv_obj_set_click(cse, true);

    lv_obj_set_size(cse, LV_DPI * 2, LV_DPI / 3);

    lv_obj_set_style(cse, &lv_style_plain);

    return cse;
}

void ysw_lv_cse_set_cs(lv_obj_t *cse, ysw_cs_t *cs)
{
    LV_ASSERT_OBJ(cse, LV_OBJX_NAME);
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->cs == cs) {
        return;
    }
    ext->cs = cs;
    lv_obj_invalidate(cse);
}

void ysw_lv_cse_select(lv_obj_t *cse, ysw_csn_t *csn, bool selected)
{
    if (selected) {
        csn->state |= YSW_CSN_SELECTED;
    } else {
        csn->state &= ~YSW_CSN_SELECTED;
    }
    lv_obj_invalidate(cse);
}

bool ysw_lv_cse_is_selected(lv_obj_t *cse, ysw_csn_t *csn)
{
    return csn->state & YSW_CSN_SELECTED;
}

