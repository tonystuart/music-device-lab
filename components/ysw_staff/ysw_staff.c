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

// TODO: include key sig and measure bars in widths

#define STAFF_HEAD 80 // TODO: calculate this, it varies based on key signature

#define WIDTH 320
#define MIDPOINT 160

typedef enum {
    DIRECTION_LEFT,
    DIRECTION_RIGHT,
} direction_t;

typedef enum {
    VISIT_DRAW,
    VISIT_MEASURE,
} visit_type_t;

typedef struct {
    lv_point_t point;
    lv_color_t color;
    ysw_array_t *patches;
    visit_type_t visit_type;
    direction_t direction;
    const lv_font_t *font;
    const lv_area_t *clip_area;
    const zm_key_signature_t *key_signature;
} dc_t;

// TODO: find the right way to share these with ysw_editor

typedef uint32_t position_x;

static inline bool is_step_position(position_x position)
{
    return position % 2 == 1;
}

static inline bool is_space_position(position_x position)
{
    return position % 2 == 0;
}

void ysw_draw_letter(const lv_point_t * pos_p, const lv_area_t * clip_area, const lv_font_t * font_p, uint32_t letter, lv_color_t color, lv_opa_t opa, lv_blend_mode_t blend_mode);

static void visit_letter(dc_t *dc, uint32_t letter)
{
    lv_font_glyph_dsc_t g;
    if (lv_font_get_glyph_dsc(dc->font, &g, letter, '\0')) {
        if (dc->direction == DIRECTION_LEFT) {
            dc->point.x -= g.adv_w;
        }
        if (dc->visit_type == VISIT_DRAW) {
            ysw_draw_letter(&dc->point, dc->clip_area, dc->font, letter, dc->color, OPA, BLEND);
        }
        if (dc->direction == DIRECTION_RIGHT) {
            dc->point.x += g.adv_w;
        }
    }
}

static void visit_label(dc_t *dc, const lv_area_t *coords, const lv_draw_label_dsc_t *dsc, const char *txt, lv_draw_label_hint_t *hint)
{
    // TODO: use visit_letter to measure x, advance y on word wrap (+ it's simpler w/fewer cycles)
    if (dc->visit_type == VISIT_DRAW) {
        lv_draw_label(coords, dc->clip_area, dsc, txt, hint);
    }
}

static void visit_measure_label(dc_t *dc, uint32_t measure)
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
    visit_label(dc, &coords, &dsc, buf, NULL);
}

static const uint8_t rest_time_base[] = {
    YSW_16_REST,
    YSW_8_REST,
    YSW_4_REST,
    YSW_2_REST,
    YSW_1_REST,
};

static void visit_rest(dc_t *dc, zm_duration_t duration)
{
    uint8_t index = 0;
    zm_round_duration(duration, &index, NULL);
    visit_letter(dc, rest_time_base[index]);
}

static const uint8_t note_time_base[] = {
    YSW_16_BASE,
    YSW_8_BASE,
    YSW_4_BASE,
    YSW_2_BASE,
    YSW_1_BASE,
};

static void visit_note(dc_t *dc, uint8_t staff_position, zm_duration_t duration)
{
    uint8_t index = 0;
    bool dotted = false;
    zm_round_duration(duration, &index, &dotted);
    if (dotted && dc->direction == DIRECTION_LEFT) {
        visit_letter(dc, YSW_DOT_BASE + staff_position);
    }
    visit_letter(dc, note_time_base[index] + staff_position);
    if (dotted && dc->direction == DIRECTION_RIGHT) {
        visit_letter(dc, YSW_DOT_BASE + staff_position);
    }
}

static void visit_black(dc_t *dc, uint8_t note, zm_duration_t duration)
{
    uint8_t accidental = 0;
    uint8_t staff_position = sharp_map[note];
    if (dc->key_signature->sharps) {
        // if this staff position is not a sharp in this key
        if (!dc->key_signature->sharp_index[staff_position % 7]) {
            // add the sharp symbol
            accidental = YSW_SHARP_BASE + staff_position;
        }
    } else if (dc->key_signature->flats) {
        staff_position = flat_map[note];
        // if this staff position is not a flat in this key
        if (!dc->key_signature->flat_index[staff_position % 7]) {
            // add the flat symbol
            accidental = YSW_FLAT_BASE + staff_position;
        }
    } else {
        // otherwise we're in the key of CMaj/Amin with no sharps/flats so add sharp symbol
        accidental = YSW_SHARP_BASE + staff_position;
    }
    if (dc->direction == DIRECTION_LEFT) {
        visit_note(dc, staff_position, duration);
    }
    if (accidental) {
        visit_letter(dc, accidental);
    }
    if (dc->direction == DIRECTION_RIGHT) {
        visit_note(dc, staff_position, duration);
    }
}

static void visit_white(dc_t *dc, uint8_t note, zm_duration_t duration)
{
    uint8_t accidental = 0;
    uint8_t staff_position = sharp_map[note];
    // if this staff position is a sharp in this key
    if (dc->key_signature->sharp_index[staff_position % 7]) {
        // add the natural symbol
        accidental = YSW_NATURAL_BASE + staff_position;
    }
    // if this staff position is a flat in this key
    else if (dc->key_signature->flat_index[staff_position % 7]) {
        staff_position = flat_map[note];
        // add the natural symbol
        accidental = YSW_NATURAL_BASE + staff_position;
    }
    if (dc->direction == DIRECTION_LEFT) {
        visit_note(dc, staff_position, duration);
    }
    if (accidental) {
        visit_letter(dc, accidental);
    }
    if (dc->direction == DIRECTION_RIGHT) {
        visit_note(dc, staff_position, duration);
    }
}

static void visit_melody(dc_t *dc, zm_step_t *step)
{
    zm_duration_t duration = step->melody.duration;
    if (step->melody.note) {
        uint8_t note = (step->melody.note - 60) % MAX_NOTES;
        if (black[note]) {
            visit_black(dc, note, duration);
        } else {
            visit_white(dc, note, duration);
        }
    } else if (duration) {
        visit_rest(dc, duration);
    }
}

static void visit_chord(dc_t *dc, zm_chord_t *chord)
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
    visit_label(dc, &coords, &dsc, zm_get_note_name(chord->root), NULL);
    coords.y1 += 9;
    visit_label(dc, &coords, &dsc, chord->type->label, NULL);
}

static void visit_rhythm(dc_t *dc, zm_rhythm_t *rhythm)
{
    lv_area_t coords = {
        .x1 = dc->point.x,
        .x2 = dc->point.x + 60,
        .y1 = 170,
        .y2 = 200,
    };
    lv_draw_label_dsc_t dsc = {
        .color = dc->color,
        .font = &lv_font_unscii_8,
        .opa = LV_OPA_COVER,
    };
    // currently display surface first because patch can be multiple lines
    if (rhythm->surface) {
        zm_patch_t *patch = zm_get_patch(dc->patches, rhythm->surface);
        if (patch && patch->name) {
            visit_label(dc, &coords, &dsc, patch->name,  NULL);
            coords.y1 += 9;
        }
    }
    if (rhythm->beat) {
        visit_label(dc, &coords, &dsc, rhythm->beat->label,  NULL);
    }
}

static void visit_signature(dc_t *dc, zm_time_signature_x time)
{
    visit_letter(dc, YSW_TIME_BASE + time);

    if (dc->key_signature->sharps) {
        visit_letter(dc, YSW_KEY_SHARP_BASE + dc->key_signature->sharps - 1);
    } else if (dc->key_signature->flats) {
        visit_letter(dc, YSW_KEY_FLAT_BASE + dc->key_signature->flats - 1);
        visit_letter(dc, YSW_STAFF_SPACE);
    } else {
        visit_letter(dc, YSW_STAFF_SPACE);
    }

    visit_letter(dc, YSW_TREBLE_CLEF);
    visit_letter(dc, YSW_LEFT_BAR);
}

static void visit_tie(dc_t *dc, zm_step_t *step)
{
    lv_coord_t x = dc->point.x;
    if (step->melody.note >= 72) {
        visit_letter(dc, YSW_TIE_UPPER);
    } else {
        visit_letter(dc, YSW_TIE_LOWER);
    }
    dc->point.x = x;
}

static void visit_step(dc_t *dc, zm_step_t *step)
{
    if (dc->direction == DIRECTION_LEFT) {
        visit_letter(dc, YSW_STAFF_SPACE);
        if (step->chord.root || step->rhythm.beat || step->rhythm.surface) {
            visit_letter(dc, YSW_STAFF_SPACE);
        }
        if (step->melody.tie) {
            visit_tie(dc, step);
        }
        visit_melody(dc, step);
        if (step->chord.root) {
            visit_chord(dc, &step->chord);
        }
        if (step->rhythm.beat || step->rhythm.surface) {
            visit_rhythm(dc, &step->rhythm);
        }
    } else if (dc->direction == DIRECTION_RIGHT) {
        if (step->chord.root) {
            visit_chord(dc, &step->chord);
        }
        if (step->rhythm.beat || step->rhythm.surface) {
            visit_rhythm(dc, &step->rhythm);
        }
        visit_melody(dc, step);
        if (step->melody.tie) {
            visit_tie(dc, step);
        }
        visit_letter(dc, YSW_STAFF_SPACE);
        if (step->chord.root || step->rhythm.beat || step->rhythm.surface) {
            visit_letter(dc, YSW_STAFF_SPACE);
        }
    }
}

static void visit_staff(ysw_staff_ext_t *ext, dc_t *dc)
{
    uint32_t step_count = ysw_array_get_count(ext->section->steps);
    uint32_t symbol_count = step_count * 2;

    dc->direction = DIRECTION_LEFT;
    dc->point.x = ext->start_x;

    for (int32_t left = ext->position - 1; left >= 0 && dc->point.x > 0; left--) {
        if (is_step_position(left)) {
            uint32_t step_index = left / 2;
            zm_step_t *step = ysw_array_get(ext->section->steps, step_index);
            if (step->flags & ZM_STEP_END_OF_MEASURE) {
                visit_letter(dc, YSW_STAFF_SPACE);
                visit_measure_label(dc, step->measure + 1);
                visit_letter(dc, YSW_RIGHT_BAR);
            }
            visit_step(dc, step);
        } else {
            visit_letter(dc, YSW_STAFF_SPACE);
        }
    }

    if (dc->point.x > 0) {
        visit_signature(dc, ext->section->time);
    }

    dc->direction = DIRECTION_RIGHT;
    dc->point.x = ext->start_x;

    for (uint32_t right = ext->position; right <= symbol_count && dc->point.x < WIDTH; right++) {
        if (is_step_position(right % 2)) {
            uint32_t step_index = right / 2;
            zm_step_t *step = ysw_array_get(ext->section->steps, step_index);
            if (right == ext->position) {
                dc->color = LV_COLOR_RED;
                visit_step(dc, step);
                dc->color = LV_COLOR_WHITE;
            } else {
                visit_step(dc, step);
            }
            if (step->flags & ZM_STEP_END_OF_MEASURE) {
                visit_letter(dc, YSW_RIGHT_BAR);
                visit_measure_label(dc, step->measure + 1);
                visit_letter(dc, YSW_STAFF_SPACE);
            }
        } else {
            if (right == ext->position) {
                dc->color = LV_COLOR_RED;
                visit_letter(dc, YSW_STAFF_SPACE);
                dc->color = LV_COLOR_WHITE;
            } else {
                visit_letter(dc, YSW_STAFF_SPACE);
            }
        }
    }
}

static void visit_main(lv_obj_t *staff, const lv_area_t *clip_area)
{
    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    if (ext->section) {
        dc_t dc = {
            .visit_type = VISIT_DRAW,
            .point.y = 70,
            .color = LV_COLOR_WHITE,
            .clip_area = clip_area,
            .font = &MusiQwikT_48,
            .key_signature = zm_get_key_signature(ext->section->key),
            .patches = ext->section->rhythm_program->patches,
        };
        visit_staff(ext, &dc);
    }
}

static lv_coord_t measure_position(ysw_staff_ext_t *ext, position_x position)
{
    lv_area_t clip_area = {
        .x2 = LV_HOR_RES_MAX,
        .y2 = LV_VER_RES_MAX,
    };
    dc_t dc = {
        .visit_type = VISIT_MEASURE,
        .direction = DIRECTION_RIGHT,
        .point.x = 0,
        .point.y = 70,
        .color = LV_COLOR_WHITE,
        .clip_area = &clip_area,
        .font = &MusiQwikT_48,
        .key_signature = zm_get_key_signature(ext->section->key),
        .patches = ext->section->rhythm_program->patches,
    };
    if (is_space_position(position)) {
        visit_letter(&dc, YSW_STAFF_SPACE);
    } else {
        // TODO: consider factoring out get_step
        zm_step_t *step = ysw_array_get(ext->section->steps, position / 2);
        visit_step(&dc, step);
    }
    return dc.point.x;
}

static lv_design_res_t on_design(lv_obj_t *staff, const lv_area_t *clip_area, lv_design_mode_t mode)
{
    if (mode == LV_DESIGN_COVER_CHK) {
        return ancestor_design(staff, clip_area, mode);
    } else if (mode == LV_DESIGN_DRAW_MAIN) {
        visit_main(staff, clip_area);
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

    lv_obj_set_size(staff, WIDTH, 120);
    return staff;
}

void ysw_staff_set_section(lv_obj_t *staff, zm_section_t *section)
{
    assert(staff);
    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    ext->position = 0;
    ext->start_x = STAFF_HEAD;
    ext->section = section;
    lv_obj_invalidate(staff);
}

lv_coord_t get_scroll_position(ysw_staff_ext_t *ext, zm_step_x new_position)
{
    lv_coord_t start_x = ext->start_x;
    zm_step_x rightmost_position = ysw_array_get_count(ext->section->steps) * 2;
    if (new_position > ext->position) {
        // moving right -- see how much is to the right of new position
        lv_coord_t to_right = 0;
        for (zm_step_x i = new_position; i <= rightmost_position && to_right < MIDPOINT; i++) {
            to_right += measure_position(ext, i);
        }
        // if we're already in the center or there's plenty to the right
        if (start_x >= MIDPOINT && to_right > MIDPOINT) {
            // stay in the center to show context on either side
            start_x = MIDPOINT;
        } else {
            // otherwise move right
            lv_coord_t last_width = 0;
            for (zm_step_x i = ext->position; i < new_position; i++) {
                last_width = measure_position(ext, i);
                start_x += last_width;
            }
            // if we're amongst the last notes on the right
            if (start_x > WIDTH) {
                // limit to the last space on the right
                start_x = WIDTH - last_width;
            }
        }
    } else if (new_position < ext->position) {
        // moving left -- see how much is to the left of new position
        lv_coord_t to_left = STAFF_HEAD;
        for (zm_step_x i = 0; i <= new_position && to_left < MIDPOINT; i++) {
            to_left += measure_position(ext, i);
        }
        // if we're already in the center or there's plenty to the left
        if (start_x <= MIDPOINT && to_left > MIDPOINT) {
            // stay in the center to show context on either side
            start_x = MIDPOINT;
        } else {
            // otherwise move left
            lv_coord_t last_width = 0;
            // make sure ext->position is valid (it may have just been deleted)
            zm_step_x limit = min(rightmost_position, ext->position);
            for (zm_step_x i = new_position; i < limit; i++) {
                last_width = measure_position(ext, i);
                start_x -= last_width;
            }
            // if we're amongst the first notes on the left
            if (start_x < STAFF_HEAD) {
                // limit to the last space on the left, including staff head
                start_x = STAFF_HEAD;
            }
        }
    }
    return start_x;
}

void ysw_staff_set_position(lv_obj_t *staff, zm_step_x new_position, ysw_staff_position_type_t type)
{
    assert(staff);
    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);

    if (type == YSW_STAFF_DIRECT) {
        ext->start_x = MIDPOINT;
    } else {
        ext->start_x = get_scroll_position(ext, new_position);
    }

    ext->position = new_position;
    lv_obj_invalidate(staff);
}

void ysw_staff_invalidate(lv_obj_t *staff)
{
    assert(staff);
    lv_obj_invalidate(staff);
}

zm_section_t *ysw_staff_get_section(lv_obj_t *staff)
{
    assert(staff);
    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    return ext->section;
}

uint32_t ysw_staff_get_position(lv_obj_t *staff)
{
    assert(staff);
    ysw_staff_ext_t *ext = lv_obj_get_ext_attr(staff);
    return ext->position;
}

