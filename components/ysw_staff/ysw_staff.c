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

#define TAG "YSW_STAFF"
#define LV_OBJX_NAME "ysw_staff"

#define MAX_NOTES 24

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

#define YSW_TIE_UPPER 0xa6
#define YSW_TIE_LOWER 0xaa

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

// See: https://en.wikipedia.org/wiki/Accidental_(music)

typedef enum {
    YSW_STAFF_LEFT,
    YSW_STAFF_RIGHT,
} draw_type_t;

typedef struct {
    lv_point_t point;
    lv_color_t color;
    ysw_array_t *patches;
    draw_type_t draw_type;
    const lv_font_t *font;
    const lv_area_t *clip_area;
    const zm_key_signature_t *key_signature;
} draw_context_t;

void ysw_draw_letter(const lv_point_t * pos_p, const lv_area_t * clip_area, const lv_font_t * font_p, uint32_t letter, lv_color_t color, lv_opa_t opa, lv_blend_mode_t blend_mode);

static void draw_letter(draw_context_t *dc, uint32_t letter)
{
    lv_font_glyph_dsc_t g;
    if (lv_font_get_glyph_dsc(dc->font, &g, letter, '\0')) {
        if (dc->draw_type == YSW_STAFF_LEFT) {
            dc->point.x -= g.adv_w;
        }
        ysw_draw_letter(&dc->point, dc->clip_area, dc->font, letter, dc->color, OPA, BLEND);
        if (dc->draw_type == YSW_STAFF_RIGHT) {
            dc->point.x += g.adv_w;
        }
    }
}

static void draw_measure_label(draw_context_t *dc, uint32_t measure)
{
    char buf[32];
    ysw_itoa(measure, buf, sizeof(buf));
    lv_area_t coords = {
        .x1 = dc->point.x - 25,
        .x2 = dc->point.x + 25,
        .y1 = 70,
        .y2 = 80,
    };
    lv_draw_label_dsc_t dsc = {
        .color = LV_COLOR_WHITE,
        .font = &lv_font_unscii_8,
        .opa = LV_OPA_COVER,
        .flag = LV_TXT_FLAG_CENTER,
    };
    lv_draw_label(&coords, dc->clip_area, &dsc, buf, NULL);
}

static const uint8_t rest_time_base[] = {
    YSW_16_REST,
    YSW_8_REST,
    YSW_4_REST,
    YSW_2_REST,
    YSW_1_REST,
};

static void draw_rest(draw_context_t *dc, zm_duration_t duration)
{
    uint8_t index = 0;
    zm_round_duration(duration, &index, NULL);
    draw_letter(dc, rest_time_base[index]);
}

static const uint8_t note_time_base[] = {
    YSW_16_BASE,
    YSW_8_BASE,
    YSW_4_BASE,
    YSW_2_BASE,
    YSW_1_BASE,
};

static void draw_note(draw_context_t *dc, uint8_t step, zm_duration_t duration)
{
    uint8_t index = 0;
    bool dotted = false;
    zm_round_duration(duration, &index, &dotted);
    if (dotted && dc->draw_type == YSW_STAFF_LEFT) {
        draw_letter(dc, YSW_DOT_BASE + step);
    }
    draw_letter(dc, note_time_base[index] + step);
    if (dotted && dc->draw_type == YSW_STAFF_RIGHT) {
        draw_letter(dc, YSW_DOT_BASE + step);
    }
}

static void draw_black(draw_context_t *dc, uint8_t note, zm_duration_t duration)
{
    uint8_t accidental = 0;
    uint8_t step = sharp_map[note];
    if (dc->key_signature->sharps) {
        // if this staff position is not a sharp in this key
        if (!dc->key_signature->sharp_index[step % 7]) {
            // add the sharp symbol
            accidental = YSW_SHARP_BASE + step;
        }
    } else if (dc->key_signature->flats) {
        uint8_t step = flat_map[note];
        // if this staff position is not a flat in this key
        if (!dc->key_signature->flat_index[step % 7]) {
            // add the flat symbol
            accidental = YSW_FLAT_BASE + step;
        }
    } else {
        // otherwise we're in the key of CMaj/Amin with no sharps/flats so add sharp symbol
        accidental = YSW_SHARP_BASE + step;
    }
    if (dc->draw_type == YSW_STAFF_LEFT) {
        draw_note(dc, step, duration);
    }
    if (accidental) {
        draw_letter(dc, accidental);
    }
    if (dc->draw_type == YSW_STAFF_RIGHT) {
        draw_note(dc, step, duration);
    }
}

static void draw_white(draw_context_t *dc, uint8_t note, zm_duration_t duration)
{
    uint8_t accidental = 0;
    uint8_t step = sharp_map[note];
    // if this staff position is a sharp in this key
    if (dc->key_signature->sharp_index[step % 7]) {
        // add the natural symbol
        accidental = YSW_NATURAL_BASE + step;
    }
    // if this staff position is a flat in this key
    else if (dc->key_signature->flat_index[step % 7]) {
        step = flat_map[note];
        // add the natural symbol
        accidental = YSW_NATURAL_BASE + step;
    }
    if (dc->draw_type == YSW_STAFF_LEFT) {
        draw_note(dc, step, duration);
    }
    if (accidental) {
        draw_letter(dc, accidental);
    }
    if (dc->draw_type == YSW_STAFF_RIGHT) {
        draw_note(dc, step, duration);
    }
}

static void draw_melody(draw_context_t *dc, zm_division_t *division)
{
    zm_duration_t duration = division->melody.duration;
    if (division->melody.note) {
        uint8_t note = (division->melody.note - 60) % MAX_NOTES;
        if (black[note]) {
            draw_black(dc, note, duration);
        } else {
            draw_white(dc, note, duration);
        }
    } else {
        draw_rest(dc, duration);
    }
}

static void draw_chord(draw_context_t *dc, zm_chord_t *chord)
{
    lv_area_t coords = {
        .x1 = dc->point.x,
        .x2 = dc->point.x + 40,
        .y1 = 40,
        .y2 = 60,
    };
    lv_draw_label_dsc_t dsc = {
        .color = dc->color,
        .font = &lv_font_unscii_8,
        .opa = LV_OPA_COVER,
    };
    lv_draw_label(&coords, dc->clip_area, &dsc, zm_get_note_name(chord->root), NULL);
    coords.y1 += 9;
    lv_draw_label(&coords, dc->clip_area, &dsc, chord->quality->label, NULL);
}

static void draw_rhythm(draw_context_t *dc, zm_rhythm_t *rhythm)
{
    lv_area_t coords = {
        .x1 = dc->point.x,
        .x2 = dc->point.x + 40,
        .y1 = 170,
        .y2 = 200,
    };
    lv_draw_label_dsc_t dsc = {
        .color = dc->color,
        .font = &lv_font_unscii_8,
        .opa = LV_OPA_COVER,
    };
    if (rhythm->beat) {
        lv_draw_label(&coords, dc->clip_area, &dsc, rhythm->beat->label,  NULL);
        coords.y1 += 9;
    }
    if (rhythm->surface) {
        zm_patch_t *patch = zm_get_patch(dc->patches, rhythm->surface);
        if (patch && patch->name) {
            lv_draw_label(&coords, dc->clip_area, &dsc, patch->name,  NULL);
        }
    }
}

static void draw_signature(draw_context_t *dc, zm_time_signature_x time)
{
    draw_letter(dc, YSW_TIME_BASE + time);

    if (dc->key_signature->sharps) {
        draw_letter(dc, YSW_KEY_SHARP_BASE + dc->key_signature->sharps - 1);
    } else if (dc->key_signature->flats) {
        draw_letter(dc, YSW_KEY_FLAT_BASE + dc->key_signature->flats - 1);
        draw_letter(dc, YSW_STAFF_SPACE);
    } else {
        draw_letter(dc, YSW_STAFF_SPACE);
    }

    draw_letter(dc, YSW_TREBLE_CLEF);
    draw_letter(dc, YSW_LEFT_BAR);
}

static void draw_tie(draw_context_t *dc, zm_division_t *division)
{
    lv_coord_t x = dc->point.x;
    if (division->melody.note >= 72) {
        draw_letter(dc, YSW_TIE_UPPER);
    } else {
        draw_letter(dc, YSW_TIE_LOWER);
    }
    dc->point.x = x;
}

static void draw_division(draw_context_t *dc, zm_division_t *division)
{
    if (dc->draw_type == YSW_STAFF_LEFT) {
        draw_letter(dc, YSW_STAFF_SPACE);
        if (division->chord.root || division->rhythm.beat || division->rhythm.surface) {
            draw_letter(dc, YSW_STAFF_SPACE);
        }
        if (division->melody.tie) {
            draw_tie(dc, division);
        }
        draw_melody(dc, division);
        if (division->chord.root) {
            draw_chord(dc, &division->chord);
        }
        if (division->rhythm.beat || division->rhythm.surface) {
            draw_rhythm(dc, &division->rhythm);
        }
    } else if (dc->draw_type == YSW_STAFF_RIGHT) {
        if (division->chord.root) {
            draw_chord(dc, &division->chord);
        }
        if (division->rhythm.beat || division->rhythm.surface) {
            draw_rhythm(dc, &division->rhythm);
        }
        draw_melody(dc, division);
        if (division->melody.tie) {
            draw_tie(dc, division);
        }
        draw_letter(dc, YSW_STAFF_SPACE);
        if (division->chord.root || division->rhythm.beat || division->rhythm.surface) {
            draw_letter(dc, YSW_STAFF_SPACE);
        }
    }
}

static void draw_staff(ysw_staff_ext_t *ext, draw_context_t *dc)
{
    uint32_t division_count = ysw_array_get_count(ext->pattern->divisions);
    uint32_t symbol_count = division_count * 2;

    dc->draw_type = YSW_STAFF_LEFT;
    dc->point.x = 160;

    for (int32_t left = ext->position - 1; left >= 0 && dc->point.x > 0; left--) {
        if (left % 2 == 1) {
            uint32_t division_index = left / 2;
            zm_division_t *division = ysw_array_get(ext->pattern->divisions, division_index);
            if (division->flags & ZM_DIVISION_END_OF_MEASURE) {
                draw_letter(dc, YSW_STAFF_SPACE);
                draw_measure_label(dc, division->measure + 1);
                draw_letter(dc, YSW_RIGHT_BAR);
            }
            draw_division(dc, division);
        } else {
            draw_letter(dc, YSW_STAFF_SPACE);
        }
    }

    if (dc->point.x > 0) {
        draw_signature(dc, ext->pattern->time);
    }

    dc->draw_type = YSW_STAFF_RIGHT;
    dc->point.x = 160;

    for (uint32_t right = ext->position; right <= symbol_count && dc->point.x < 320; right++) {
        if (right % 2 == 1) {
            uint32_t division_index = right / 2;
            zm_division_t *division = ysw_array_get(ext->pattern->divisions, division_index);
            if (right == ext->position) {
                dc->color = LV_COLOR_RED;
                draw_division(dc, division);
                dc->color = LV_COLOR_WHITE;
            } else {
                draw_division(dc, division);
            }
            if (division->flags & ZM_DIVISION_END_OF_MEASURE) {
                draw_letter(dc, YSW_RIGHT_BAR);
                draw_measure_label(dc, division->measure + 1);
                draw_letter(dc, YSW_STAFF_SPACE);
            }
        } else {
            if (right == ext->position) {
                dc->color = LV_COLOR_RED;
                draw_letter(dc, YSW_STAFF_SPACE);
                dc->color = LV_COLOR_WHITE;
            } else {
                draw_letter(dc, YSW_STAFF_SPACE);
            }
        }
    }
}

static void draw_main(lv_obj_t *staff, const lv_area_t *clip_area)
{
    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    if (ext->pattern) {
        draw_context_t dc = {
            .point.y = 70,
            .color = LV_COLOR_WHITE,
            .clip_area = clip_area,
            .font = &MusiQwikT_48,
            .key_signature = zm_get_key_signature(ext->pattern->key),
            .patches = ext->pattern->rhythm_program->patches,
        };
        draw_staff(ext, &dc);
    }
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

void ysw_staff_set_pattern(lv_obj_t *staff, zm_pattern_t *pattern)
{
    assert(staff);
    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    ext->pattern = pattern;
    lv_obj_invalidate(staff);
}

void ysw_staff_set_position(lv_obj_t *staff, uint32_t position)
{
    assert(staff);
    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    ext->position = position;
    lv_obj_invalidate(staff);
}

void ysw_staff_invalidate(lv_obj_t *staff)
{
    assert(staff);
    lv_obj_invalidate(staff);
}

zm_pattern_t *ysw_staff_get_pattern(lv_obj_t *staff)
{
    assert(staff);
    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    return ext->pattern;
}

uint32_t ysw_staff_get_position(lv_obj_t *staff)
{
    assert(staff);
    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    return ext->position;
}

