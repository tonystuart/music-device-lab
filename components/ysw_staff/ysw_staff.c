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
#define YSW_KEY_SHARP_BASE 0x0a
#define YSW_KEY_FLAT_BASE 0x11
#define YSW_TIME_BASE 0x18
#define YSW_16_BASE 0x20
#define YSW_8_BASE 0x2e
#define YSW_4_BASE 0x3c
#define YSW_2_BASE 0x4a
#define YSW_1_BASE 0x58
#define YSW_DOT_BASE 0x66
#define YSW_SHARP_BASE 0x74
#define YSW_FLAT_BASE 0x82
#define YSW_NATURAL_BASE 0x90
#define YSW_16_REST 0xa0
#define YSW_8_REST 0xa1
#define YSW_4_REST 0xa2
#define YSW_2_REST 0xa3
#define YSW_1_REST 0xa4

#define OPA LV_OPA_100
#define BLEND LV_BLEND_MODE_NORMAL

static lv_design_cb_t ancestor_design;
static lv_signal_cb_t ancestor_signal;

static const bool black[] = {
    0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0,
};

// sharps map to the note of the white key to the left
static const uint8_t sharp_map[] = {
    0,  // C   0
    0,  // C#  1
    1,  // D   2
    1,  // D#  3
    2,  // E   4
    3,  // F   5
    3,  // F#  6
    4,  // G   7
    4,  // G#  8
    5,  // A   9
    5,  // A#  10
    6,  // B   11
    7,  // C   12
    7,  // C#  13
    8,  // D   14
    8,  // D#  15
    9,  // E   16
    10, // F   17
    10, // F#  18
    11, // G   19
    11, // G#  20
    12, // A   21
    12, // A#  22
    13, // B   23
};

// flats map to the note of the white key to the right
static const uint8_t flat_map[] = {
    0,  // C   0
    1,  // Db  1
    1,  // D   2
    2,  // Eb  3
    2,  // E   4
    3,  // F   5
    4,  // Gb  6
    4,  // G   7
    5,  // Ab  8
    5,  // A   9
    6,  // Bb  10
    6,  // B   11
    7,  // C   12
    8,  // Db  13
    8,  // D   14
    9,  // Eb  15
    9,  // E   16
    10, // F   17
    11, // Gb  18
    11, // G   19
    12, // Ab  20
    12, // A   21
    13, // Bb  22
    13, // B   23
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

#if 0
static void visit_string(visit_context_t *vc, const char *string)
{
    const char *p = string;
    while (*p) {
        visit_letter(vc, *p++);
    }
}
#endif

static void draw_measure_label(visit_context_t *vc, uint32_t measure)
{
    if (vc->visit_type == VT_DRAW) {
        char buf[32];
        ysw_itoa(measure, buf, sizeof(buf));
        lv_area_t coords = {
            .x1 = vc->point.x - 25,
            .x2 = vc->point.x + 25,
            .y1 = 70,
            .y2 = 80,
        };
        lv_draw_label_dsc_t dsc = {
            .color = LV_COLOR_WHITE,
            .font = &lv_font_unscii_8,
            .opa = LV_OPA_COVER,
            .flag = LV_TXT_FLAG_CENTER,
        };
        lv_draw_label(&coords, vc->clip_area, &dsc, buf, NULL);
    }
}

static zm_duration_t round_duration(zm_duration_t duration)
{
    // TODO: added dotted durations, consider using durations table
    zm_duration_t rounded_duration = 0;
    if (duration <= (ZM_SIXTEENTH + ((ZM_EIGHTH - ZM_SIXTEENTH) / 2))) {  // 64 + 32
        rounded_duration = ZM_SIXTEENTH;
    } else if (duration <= (ZM_EIGHTH + ((ZM_QUARTER - ZM_EIGHTH) / 2))) { // 128 + 64
        rounded_duration = ZM_EIGHTH;
    } else if (duration <= (ZM_QUARTER + ((ZM_HALF - ZM_QUARTER) / 2))) { // 256 + 128
        rounded_duration = ZM_QUARTER;
    } else if (duration <= (ZM_HALF + ((ZM_WHOLE - ZM_HALF) / 2 ))) {     // 512 + 256
        rounded_duration = ZM_HALF;
    } else {
        rounded_duration = ZM_WHOLE;
    }
    return rounded_duration;
}

static uint32_t visit_tone(visit_context_t *vc, zm_beat_t *beat, const zm_key_signature_t *key_signature)
{
    zm_duration_t duration = round_duration(beat->tone.duration);
    if (beat->tone.note) {
        uint8_t note = (beat->tone.note - 60) % 24;
        uint8_t step = sharp_map[note]; // either map works if white key
        // TODO: keep a table of accidentals for current measure
        if (black[note]) {
            if (key_signature->sharps) {
                if (!key_signature->sharp_index[step % 7]) {
                    visit_letter(vc, YSW_SHARP_BASE + step);
                }
            } else if (key_signature->flats) {
                step = flat_map[note];
                if (!key_signature->flat_index[step % 7]) {
                    visit_letter(vc, YSW_FLAT_BASE + step);
                }
            } else {
                visit_letter(vc, YSW_SHARP_BASE + step);
            }
        } else {
            if (key_signature->sharp_index[step % 7]) {
                visit_letter(vc, YSW_NATURAL_BASE + step);
            } else if (key_signature->flat_index[step % 7]) {
                step = flat_map[note];
                visit_letter(vc, YSW_NATURAL_BASE + step);
            } else {
            }
        }
        if (duration <= ZM_SIXTEENTH) {
            visit_letter(vc, YSW_16_BASE + step);
        } else if (duration <= ZM_EIGHTH) {
            visit_letter(vc, YSW_8_BASE + step);
        } else if (duration <= ZM_QUARTER) {
            visit_letter(vc, YSW_4_BASE + step);
        } else if (duration <= ZM_HALF) {
            visit_letter(vc, YSW_2_BASE + step);
        } else {
            visit_letter(vc, YSW_1_BASE + step);
        }
    } else {
        if (duration <= ZM_SIXTEENTH) {
            visit_letter(vc, YSW_16_REST);
        } else if (duration <= ZM_EIGHTH) {
            visit_letter(vc, YSW_8_REST);
        } else if (duration <= ZM_QUARTER) {
            visit_letter(vc, YSW_4_REST);
        } else if (duration <= ZM_HALF) {
            visit_letter(vc, YSW_2_REST);
        } else {
            visit_letter(vc, YSW_1_REST);
        }
    }
    return duration;
}

static void visit_chord(visit_context_t *vc, zm_beat_t *beat)
{
    if (vc->visit_type == VT_DRAW && beat->chord.root) {
        lv_area_t coords = {
            .x1 = vc->point.x - 25,
            .x2 = vc->point.x + 25,
            .y1 = 40,
            .y2 = 60,
        };
        lv_draw_label_dsc_t dsc = {
            .color = LV_COLOR_WHITE,
            .font = &lv_font_montserrat_14,
            .opa = LV_OPA_COVER,
            .flag = LV_TXT_FLAG_CENTER,
        };
        const char *note_name = zm_get_note_name(beat->chord.root);
        lv_draw_label(&coords, vc->clip_area, &dsc, note_name, NULL);
        // TODO: display quality
    }
}

static void visit_all(ysw_staff_ext_t *ext, visit_context_t *vc)
{
    visit_letter(vc, YSW_LEFT_BAR);
    visit_letter(vc, YSW_TREBLE_CLEF);

    const zm_key_signature_t *key_signature = zm_get_key_signature(ext->passage->key);
    if (key_signature->sharps) {
        visit_letter(vc, YSW_KEY_SHARP_BASE + key_signature->sharps - 1);
    } else if (key_signature->flats) {
        visit_letter(vc, YSW_STAFF_SPACE);
        visit_letter(vc, YSW_KEY_FLAT_BASE + key_signature->flats - 1);
    } else {
        visit_letter(vc, YSW_STAFF_SPACE);
    }

    visit_letter(vc, YSW_TIME_BASE + ext->passage->time);

    uint32_t ticks_in_measure = 0;
    uint32_t beat_count = ysw_array_get_count(ext->passage->beats);
    uint32_t symbol_count = beat_count * 2;
    uint32_t measure = 1;

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
            visit_chord(vc, beat);
            ticks_in_measure += visit_tone(vc, beat, key_signature);
            if (ticks_in_measure >= 1024) {
                measure++;
                visit_letter(vc, YSW_RIGHT_BAR);
                draw_measure_label(vc, measure);
                ticks_in_measure = 0;
            }
        }
        if (i == ext->position) {
            vc->color = LV_COLOR_WHITE;
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

