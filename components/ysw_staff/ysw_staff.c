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
#include "ysw_style.h"
#include "lvgl.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "STAFF"
#define LV_OBJX_NAME "ysw_staff"

#define YSW_TREBLE_CLEF 0x01
#define YSW_STAFF_SPACE 0x02
#define YSW_LEFT_BAR 0x03
#define YSW_RIGHT_BAR 0x04
#define YSW_2_2_TIME 0x18
#define YSW_2_4_TIME 0x19
#define YSW_3_4_TIME 0x1a
#define YSW_4_4_TIME 0x1b
#define YSW_3_2_TIME 0x1c
#define YSW_6_8_TIME 0x1d
#define YSW_16_BASE 0x20
#define YSW_8_BASE 0x2e
#define YSW_4_BASE 0x3c
#define YSW_2_BASE 0x4a
#define YSW_1_BASE 0x58
#define YSW_DOT_BASE 0x66
#define YSW_SHARP_BASE 0x74
#define YSW_FLAT_BASE 0x82
#define YSW_16_REST 0xa0
#define YSW_8_REST 0xa1
#define YSW_4_REST 0xa2
#define YSW_2_REST 0xa3
#define YSW_1_REST 0xa4

#define OPA LV_OPA_100
#define BLEND LV_BLEND_MODE_NORMAL

static lv_design_cb_t ancestor_design;
static lv_signal_cb_t ancestor_signal;

typedef struct {
    uint8_t step;
    bool half;
} semitones_t;

static const semitones_t semitones[] = {
    { 0, false },  // C   0
    { 0, true },   // C#  1
    { 1, false },  // D   2
    { 1, true },   // D#  3
    { 2, false },  // E   4
    { 3, false },  // F   5
    { 3, true },   // F#  6
    { 4, false },  // G   7
    { 4, true },   // G#  8
    { 5, false },  // A   9
    { 5, true },   // A#  10
    { 6, false },  // B   11
    { 7, false },  // C   12
    { 7, true },   // C#  13
    { 8, false },  // D   14
    { 8, true },   // D#  15
    { 9, false },  // E   16
    { 10, false }, // F   17
    { 10, true },  // F#  18
    { 11, false }, // G   19
    { 11, true },  // G#  20
    { 12, false }, // A   21
    { 12, true },  // A#  22
    { 13, false }, // B   23
};

typedef enum {
    VT_MEASURE,
    VT_DRAW,
} visit_type_t;

typedef struct {
    lv_point_t point;
    lv_color_t color;
    const lv_area_t *clip_area;
    const lv_font_t *font;
    visit_type_t visit_type;
} visit_context_t;

void ysw_draw_letter(const lv_point_t * pos_p, const lv_area_t * clip_area, const lv_font_t * font_p, uint32_t letter, lv_color_t color, lv_opa_t opa, lv_blend_mode_t blend_mode);

static void visit_letter(visit_context_t *vc, uint32_t letter)
{
    lv_font_glyph_dsc_t g;
    if (lv_font_get_glyph_dsc(vc->font, &g, letter, '\0')) {
        if (vc->visit_type == VT_DRAW) {
            ysw_draw_letter(&vc->point, vc->clip_area, vc->font, letter, vc->color, OPA, BLEND);
        }
        vc->point.x += g.adv_w;
    }
}

static void visit_string(visit_context_t *vc, const char *string)
{
    const char *p = string;
    while (*p) {
        visit_letter(vc, *p++);
    }
}

static void visit_all(ysw_staff_ext_t *ext, visit_context_t *vc)
{
    visit_letter(vc, YSW_LEFT_BAR);
    visit_letter(vc, YSW_TREBLE_CLEF);

    const zm_key_t *key = zm_get_key(ext->passage->key);
    if (key->sharps) {
        visit_letter(vc, 0x0a + key->sharps - 1);
    } else if (key->flats) {
        visit_letter(vc, YSW_STAFF_SPACE);
        visit_letter(vc, 0x11 + key->flats - 1);
    } else {
        visit_letter(vc, YSW_STAFF_SPACE);
    }

    visit_letter(vc, YSW_4_4_TIME);

    uint32_t ticks_in_measure = 0;
    uint32_t beat_count = ysw_array_get_count(ext->passage->beats);
    uint32_t symbol_count = beat_count * 2;

    for (int i = 0; i <= symbol_count; i++) { // = to get trailing space
        if (i == ext->position) {
            if (vc->visit_type == VT_MEASURE) {
                return;
            }
            vc->color = LV_COLOR_RED;
        }
        if (i % 2 == 0) {
            visit_letter(vc, YSW_STAFF_SPACE);
        } else {
            uint32_t beat_index = i / 2;
            zm_beat_t *beat = ysw_array_get(ext->passage->beats, beat_index);
            ticks_in_measure += beat->tone.duration;
            if (beat->tone.note) {
                uint8_t note = (beat->tone.note - 60) % 24;
                uint8_t step = semitones[note].step;
                if (semitones[note].half) {
                    visit_letter(vc, YSW_SHARP_BASE + step);
                }
                if (beat->tone.duration <= ZM_SIXTEENTH) {
                    visit_letter(vc, YSW_16_BASE + step);
                } else if (beat->tone.duration <= ZM_EIGHTH) {
                    visit_letter(vc, YSW_8_BASE + step);
                } else if (beat->tone.duration <= ZM_QUARTER) {
                    visit_letter(vc, YSW_4_BASE + step);
                } else if (beat->tone.duration <= ZM_HALF) {
                    visit_letter(vc, YSW_2_BASE + step);
                } else {
                    visit_letter(vc, YSW_1_BASE + step);
                }
            } else {
                if (beat->tone.duration <= ZM_SIXTEENTH) {
                    visit_letter(vc, YSW_16_REST);
                } else if (beat->tone.duration <= ZM_EIGHTH) {
                    visit_letter(vc, YSW_8_REST);
                } else if (beat->tone.duration <= ZM_QUARTER) {
                    visit_letter(vc, YSW_4_REST);
                } else if (beat->tone.duration <= ZM_HALF) {
                    visit_letter(vc, YSW_2_REST);
                } else {
                    visit_letter(vc, YSW_1_REST);
                }
            }
        }
        if (i == ext->position) {
            vc->color = LV_COLOR_WHITE;
        }
        if (ticks_in_measure >= 1024) {
            visit_letter(vc, YSW_RIGHT_BAR);
            ticks_in_measure = 0;
        }
    }
}

static void draw_main(lv_obj_t *staff, const lv_area_t *clip_area)
{
    lv_draw_rect_dsc_t rect_dsc = {
        .bg_color = LV_COLOR_BLACK,
        .bg_opa = LV_OPA_COVER,
    };
    lv_area_t coords;
    lv_obj_get_coords(staff, &coords);
    lv_draw_rect(&coords, clip_area, &rect_dsc);

    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);

    if (!ext->passage) {
        return;
    }

    visit_context_t vc = {
        .point.y = 60,
        .color = LV_COLOR_WHITE,
        .clip_area = clip_area,
        .font = &MusiQwikT_48,
    };

    if (ext->last_x) {
        vc.point.x = ext->last_x;
    } else {
        vc.point.x = 0;
        vc.visit_type = VT_MEASURE;
        visit_all(ext, &vc);
        vc.point.x = 160 - vc.point.x;
        ext->last_x = vc.point.x;
    }

    vc.visit_type = VT_DRAW;
    visit_all(ext, &vc);
}

static lv_design_res_t on_design(lv_obj_t *staff, const lv_area_t *clip_area, lv_design_mode_t mode)
{
    if (mode == LV_DESIGN_COVER_CHK) {
        return ancestor_design(staff, clip_area, mode);
    } else if (mode == LV_DESIGN_DRAW_MAIN) {
        draw_main(staff, clip_area);
    }
    return LV_DESIGN_RES_OK;
}

static lv_res_t on_signal(lv_obj_t *staff, lv_signal_t sign, void *param)
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

lv_obj_t *ysw_staff_create(lv_obj_t *par)
{
    lv_obj_t *staff = lv_obj_create(par, NULL);
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

    memset(ext, 0, sizeof(ysw_staff_ext_t));

    lv_obj_set_signal_cb(staff, on_signal);
    lv_obj_set_design_cb(staff, on_design);

    lv_obj_set_size(staff, 320, 120);
    lv_theme_apply(staff, LV_THEME_LED);

    return staff;
}

void ysw_staff_set_passage(lv_obj_t *staff, zm_passage_t *passage)
{
    assert(staff);
    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    ext->passage = passage;
    ext->last_x = 0;
    lv_obj_invalidate(staff);
}

void ysw_staff_update_position(lv_obj_t *staff, uint32_t position)
{
    assert(staff);
    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    ext->position = position;
    ext->last_x = 0;
    lv_obj_invalidate(staff);
}

void ysw_staff_update_all(lv_obj_t *staff, uint32_t position)
{
    assert(staff);
    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    ext->position = position;
    ext->last_x = 0;
    lv_obj_invalidate(staff);
}

zm_passage_t *ysw_staff_get_passage(lv_obj_t *staff)
{
    assert(staff);
    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    return ext->passage;
}

uint32_t ysw_staff_get_position(lv_obj_t *staff)
{
    assert(staff);
    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    return ext->position;
}

