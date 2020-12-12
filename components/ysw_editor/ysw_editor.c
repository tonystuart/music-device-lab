// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_editor.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_footer.h"
#include "ysw_header.h"
#include "ysw_heap.h"
#include "ysw_menu.h"
#include "ysw_staff.h"
#include "ysw_task.h"
#include "ysw_ticks.h"
#include "zm_music.h"
#include "lvgl.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_EDITOR"

#define DEFAULT_QUALITY 2 // TODO: make "major" 0 default or search by name
#define DEFAULT_STYLE 2 // TODO: make "stacked" 0 default or search by name
#define DEFAULT_BEAT 0 // TODO: search by name

// channels

#define BASE_CHANNEL 0
#define MELODY_CHANNEL (BASE_CHANNEL+0)
#define CHORD_CHANNEL (BASE_CHANNEL+1)
#define RHYTHM_CHANNEL (BASE_CHANNEL+2)

#define BACKGROUND_BASE 3

// TODO: change to NOTE, CHORD, BEAT

typedef enum {
    YSW_EDITOR_MODE_MELODY,
    YSW_EDITOR_MODE_CHORD,
    YSW_EDITOR_MODE_RHYTHM,
} ysw_editor_mode_t;

static const char *modes[] = {
    "Melody",
    "Chord",
    "Rhythm",
};

#define YSW_EDITOR_MODE_COUNT 3

typedef struct {
    ysw_bus_t *bus;
    ysw_editor_lvgl_init lvgl_init;

    zm_music_t *music;
    zm_pattern_t *pattern;
    zm_quality_t *quality;
    zm_style_t *style;
    zm_beat_t *beat;
    zm_duration_t duration;

    zm_time_x down_at;
    zm_time_x up_at;
    zm_time_x delta;

    bool loop;
    uint8_t advance;
    uint32_t position;
    ysw_editor_mode_t mode;

    lv_obj_t *container;
    lv_obj_t *header;
    lv_obj_t *staff;
    lv_obj_t *footer;

    ysw_menu_t *menu;
} ysw_editor_t;

// Note that position is 2x the division index and encodes both the division index and the space before it.

// 0 % 2 = 0
// 1 % 2 = 1 C
// 2 % 2 = 0
// 3 % 2 = 1 E
// 4 % 2 = 0
// 5 % 2 = 1 G
// 6 % 2 = 0

// A remainder of zero indicates the space and a remainder of 1 indicates
// the division. This happens to work at the end of the array too, if the position
// is 2x the size of the array, it refers to the space following the last division.

// if (position / 2 >= size) {
//   -> space after last division
// } else {
//   if (position % 2 == 0) {
//     -> space before division
//   } else {
//     -> division_index = position / 2;
//   }
// }

static inline bool is_division_position(ysw_editor_t *ysw_editor)
{
    return ysw_editor->position % 2 == 1;
}

static inline bool is_space_position(ysw_editor_t *ysw_editor)
{
    return ysw_editor->position % 2 == 0;
}
 
static inline uint32_t division_index_to_position(zm_division_x division_index)
{
    return (division_index * 2) + 1;
}

static zm_division_t *get_division(ysw_editor_t *ysw_editor)
{
    zm_division_t *division = NULL;
    if (is_division_position(ysw_editor)) {
        zm_division_x division_index = ysw_editor->position / 2;
        division = ysw_array_get(ysw_editor->pattern->divisions, division_index);
    }
    return division;
}

// TODO: remove unnecessary calls to recalculate (e.g. increase/decrease pitch)

static void recalculate(ysw_editor_t *ysw_editor)
{
    zm_time_x start = 0;
    zm_measure_x measure = 1;
    uint32_t ticks_in_measure = 0;

    zm_division_t *division = NULL;
    zm_division_t *previous = NULL;

    zm_time_x ticks_per_measure = zm_get_ticks_per_measure(ysw_editor->pattern->time);
    zm_division_x division_count = ysw_array_get_count(ysw_editor->pattern->divisions);

    for (zm_division_x i = 0; i < division_count; i++) {
        division = ysw_array_get(ysw_editor->pattern->divisions, i);
        division->start = start;
        division->flags = 0;
        division->measure = measure;
        if (division->chord.root) {
            if (previous) {
                zm_time_x chord_delta_time = start - previous->start;
                previous->chord.duration = min(chord_delta_time, ticks_per_measure);
            }
            previous = division;
        }
        ticks_in_measure += zm_round_duration(division->melody.duration, NULL, NULL);
        if (ticks_in_measure >= ticks_per_measure) {
            division->flags |= ZM_DIVISION_END_OF_MEASURE;
            ticks_in_measure = 0;
            measure++;
        }
        start += division->melody.duration;
    }

    if (previous) {
        if (previous == division) {
            // piece ends on this chord, set it's duration to what's left in the measure
            previous->chord.duration = (previous->measure * ticks_per_measure) - previous->start;
        } else {
            // otherwise piece ends after previous chord, adjust previous chord's duration
            zm_time_x chord_delta_time = start - previous->start;
            previous->chord.duration = min(chord_delta_time, ticks_per_measure);
        }
    }

    ysw_staff_invalidate(ysw_editor->staff);
}

static void play_division(ysw_editor_t *ysw_editor, zm_division_t *division)
{
    ysw_array_t *notes = zm_render_division(ysw_editor->music, ysw_editor->pattern, division, BASE_CHANNEL);
    zm_bpm_x bpm = zm_tempo_to_bpm(ysw_editor->pattern->tempo);
    ysw_event_fire_play(ysw_editor->bus, notes, bpm);
}

static void play_position(ysw_editor_t *ysw_editor)
{
    if (is_division_position(ysw_editor)) {
        zm_division_x division_index = ysw_editor->position / 2;
        zm_division_t *division = ysw_array_get(ysw_editor->pattern->divisions, division_index);
        play_division(ysw_editor, division);
    }
}

static void display_program(ysw_editor_t *ysw_editor)
{
    const char *value = "";
    if (ysw_editor->mode == YSW_EDITOR_MODE_MELODY) {
        value = ysw_editor->pattern->melody_program->name;
    } else if (ysw_editor->mode == YSW_EDITOR_MODE_CHORD) {
        value = ysw_editor->pattern->chord_program->name;
    } else if (ysw_editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        value = ysw_editor->pattern->rhythm_program->name;
    }
    ysw_header_set_program(ysw_editor->header, value);
}

static void display_melody_mode(ysw_editor_t *ysw_editor)
{
    // 1. on a division - show note or rest
    // 2. between divisions - show previous (to left) note or rest
    // 3. no divisions - show blank
    char value[32];
    zm_division_x division_count = ysw_array_get_count(ysw_editor->pattern->divisions);
    if (ysw_editor->position > 0) {
        zm_division_x division_index = ysw_editor->position / 2;
        if (division_index >= division_count) {
            division_index = division_count - 1;
        }
        zm_bpm_x bpm = zm_tempo_to_bpm(ysw_editor->pattern->tempo);
        zm_division_t *division = ysw_array_get(ysw_editor->pattern->divisions, division_index);
        uint32_t millis = ysw_ticks_to_millis(division->melody.duration, bpm);
        zm_note_t note = division->melody.note;
        if (note) {
            uint8_t octave = (note / 12) - 1;
            const char *name = zm_get_note_name(note);
            snprintf(value, sizeof(value), "%s%d (%d ms)", name, octave, millis);
        } else {
            snprintf(value, sizeof(value), "Rest (%d ms)", millis);
        }
    } else {
        snprintf(value, sizeof(value), "%s", ysw_editor->pattern->name);
    }
    ysw_header_set_mode(ysw_editor->header, modes[ysw_editor->mode], value);
}

static void display_chord_mode(ysw_editor_t *ysw_editor)
{
    // 1. on a division with a chord value - show chord value, quality, style
    // 2. on a division without a chord value - show quality, style
    // 3. between divisions - show quality, style
    // 4. no divisions - show quality, style
    char value[32] = {};
    zm_division_t *division = NULL;
    if (is_division_position(ysw_editor)) {
        division = ysw_array_get(ysw_editor->pattern->divisions, ysw_editor->position / 2);
    }
    if (division && division->chord.root) {
        snprintf(value, sizeof(value), "%s %s %s",
                zm_get_note_name(division->chord.root),
                division->chord.quality->name,
                division->chord.style->name);
    } else {
        snprintf(value, sizeof(value), "%s %s",
                ysw_editor->quality->name,
                ysw_editor->style->name);
    }
    ysw_header_set_mode(ysw_editor->header, modes[ysw_editor->mode], value);
}

static void display_rhythm_mode(ysw_editor_t *ysw_editor)
{
    char value[32] = {};
    zm_division_t *division = NULL;
    if (is_division_position(ysw_editor)) {
        division = ysw_array_get(ysw_editor->pattern->divisions, ysw_editor->position / 2);
    }
    if (division && division->rhythm.beat) {
        snprintf(value, sizeof(value), "%s", division->rhythm.beat->name);
    } else {
        snprintf(value, sizeof(value), "%s", ysw_editor->beat->name);
    }
    ysw_header_set_mode(ysw_editor->header, modes[ysw_editor->mode], value);
}

static void display_mode(ysw_editor_t *ysw_editor)
{
    switch (ysw_editor->mode) {
        case YSW_EDITOR_MODE_MELODY:
            display_melody_mode(ysw_editor);
            break;
        case YSW_EDITOR_MODE_CHORD:
            display_chord_mode(ysw_editor);
            break;
        case YSW_EDITOR_MODE_RHYTHM:
            display_rhythm_mode(ysw_editor);
            break;
    }
    // program is mode specific
    display_program(ysw_editor);
}

// TODO: Move to zm_music (or move the other "next" functions here)

static zm_program_t *next_program(ysw_array_t *programs, zm_program_t *current, zm_program_type_t type)
{
    zm_program_t *next = current;
    zm_program_x program_count = ysw_array_get_count(programs);
    zm_program_x program_index = ysw_array_find(programs, current);
    for (zm_program_x i = 0; i < program_count && next == current; i++) {
        program_index = (program_index + 1) % program_count;
        zm_program_t *program = ysw_array_get(programs, program_index);
        if (program->type == type) {
            next = program;
        }
    }
    return next;
}

static void fire_program_change(ysw_editor_t *ysw_editor, zm_program_t *program, zm_channel_x channel)
{
    ysw_event_program_change_t program_change = {
        .channel = channel,
        .program = ysw_array_find(ysw_editor->music->programs, program),
    };
    ysw_event_fire_program_change(ysw_editor->bus, YSW_ORIGIN_EDITOR, &program_change);
}

static void cycle_program(ysw_editor_t *ysw_editor)
{
    if (ysw_editor->mode == YSW_EDITOR_MODE_MELODY) {
        ysw_editor->pattern->melody_program = next_program(ysw_editor->music->programs,
                ysw_editor->pattern->melody_program, ZM_PROGRAM_NOTE);
        fire_program_change(ysw_editor, ysw_editor->pattern->melody_program, MELODY_CHANNEL);
    } else if (ysw_editor->mode == YSW_EDITOR_MODE_CHORD) {
        ysw_editor->pattern->chord_program = next_program(ysw_editor->music->programs,
                ysw_editor->pattern->chord_program, ZM_PROGRAM_NOTE);
        fire_program_change(ysw_editor, ysw_editor->pattern->chord_program, CHORD_CHANNEL);
    } else if (ysw_editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        ysw_editor->pattern->rhythm_program = next_program(ysw_editor->music->programs,
                ysw_editor->pattern->rhythm_program, ZM_PROGRAM_BEAT);
        fire_program_change(ysw_editor, ysw_editor->pattern->rhythm_program, RHYTHM_CHANNEL);
    }
    display_program(ysw_editor);
    play_position(ysw_editor);
}

static void cycle_key_signature(ysw_editor_t *ysw_editor)
{
    ysw_editor->pattern->key = zm_get_next_key_index(ysw_editor->pattern->key);
    recalculate(ysw_editor);
    ysw_footer_set_key(ysw_editor->footer, ysw_editor->pattern->key);
}

static void cycle_time_signature(ysw_editor_t *ysw_editor)
{
    ysw_editor->pattern->time = zm_get_next_time_index(ysw_editor->pattern->time);
    recalculate(ysw_editor);
    ysw_footer_set_time(ysw_editor->footer, ysw_editor->pattern->time);
}

static void cycle_tempo(ysw_editor_t *ysw_editor)
{
    ysw_editor->pattern->tempo = zm_get_next_tempo_index(ysw_editor->pattern->tempo);
    recalculate(ysw_editor);
    ysw_footer_set_tempo(ysw_editor->footer, ysw_editor->pattern->tempo);
    display_mode(ysw_editor);
}

static void cycle_duration(ysw_editor_t *ysw_editor)
{
    zm_division_t *division = NULL;
    if (is_division_position(ysw_editor) && ysw_editor->duration != ZM_AS_PLAYED) {
        zm_division_x division_index = ysw_editor->position / 2;
        division = ysw_array_get(ysw_editor->pattern->divisions, division_index);
    }
    // Cycle default duration if not on a division or if division duration is already default duration
    if (!division || division->melody.duration == ysw_editor->duration) {
        ysw_editor->duration = zm_get_next_duration(ysw_editor->duration);
        ysw_footer_set_duration(ysw_editor->footer, ysw_editor->duration);
    }
    if (division) {
        division->melody.duration = ysw_editor->duration;
        recalculate(ysw_editor);
        display_mode(ysw_editor);
    }
}

static void cycle_tie(ysw_editor_t *ysw_editor)
{
    if (is_division_position(ysw_editor)) {
        zm_division_x division_index = ysw_editor->position / 2;
        zm_division_t *division = ysw_array_get(ysw_editor->pattern->divisions, division_index);
        if (division->melody.note) {
            if (division->melody.tie) {
                division->melody.tie = 0;
            } else {
                division->melody.tie = 1;
            }
            ysw_staff_invalidate(ysw_editor->staff);
        }
    }
}

static zm_style_t *find_matching_style(ysw_editor_t *ysw_editor, bool preincrement)
{

    zm_style_x current = ysw_array_find(ysw_editor->music->styles, ysw_editor->style);
    zm_style_x style_count = ysw_array_get_count(ysw_editor->music->styles);
    zm_distance_x distance_count = ysw_array_get_count(ysw_editor->quality->distances);
    for (zm_style_x i = 0; i < style_count; i++) {
        if (preincrement) {
            current = (current + 1) % style_count;
        }
        zm_style_t *style = ysw_array_get(ysw_editor->music->styles, current);
        if (style->distance_count == distance_count) {
            ysw_editor->style = style;
            return style;
        }
        if (!preincrement) {
            current = (current + 1) % style_count;
        }
    }
    assert(false);
}

static void cycle_quality(ysw_editor_t *ysw_editor)
{
    zm_division_t *division = NULL;
    if (is_division_position(ysw_editor)) {
        zm_division_x division_index = ysw_editor->position / 2;
        division = ysw_array_get(ysw_editor->pattern->divisions, division_index);
    }
    if (!division || !division->chord.root || division->chord.quality == ysw_editor->quality) {
        zm_quality_x previous = ysw_array_find(ysw_editor->music->qualities, ysw_editor->quality);
        zm_quality_x quality_count = ysw_array_get_count(ysw_editor->music->qualities);
        zm_quality_x next = (previous + 1) % quality_count;
        ysw_editor->quality = ysw_array_get(ysw_editor->music->qualities, next);
        ysw_editor->style = find_matching_style(ysw_editor, false);
    }
    if (division) {
        division->chord.quality = ysw_editor->quality;
        division->chord.style = ysw_editor->style;
        ysw_staff_invalidate(ysw_editor->staff);
    }
    display_mode(ysw_editor);
    play_position(ysw_editor);
}

static void cycle_style(ysw_editor_t *ysw_editor)
{
    zm_division_t *division = NULL;
    if (is_division_position(ysw_editor)) {
        zm_division_x division_index = ysw_editor->position / 2;
        division = ysw_array_get(ysw_editor->pattern->divisions, division_index);
    }
    if (!division || !division->chord.root || division->chord.style == ysw_editor->style) {
        ysw_editor->style = find_matching_style(ysw_editor, true);
    }
    if (division) {
        division->chord.style = ysw_editor->style;
    }
    display_mode(ysw_editor);
    play_position(ysw_editor);
}

static void cycle_beat(ysw_editor_t *ysw_editor)
{
    zm_division_t *division = NULL;
    if (is_division_position(ysw_editor)) {
        zm_division_x division_index = ysw_editor->position / 2;
        division = ysw_array_get(ysw_editor->pattern->divisions, division_index);
    }
    if (!division || !division->rhythm.beat || division->rhythm.beat == ysw_editor->beat) {
        zm_beat_x previous = ysw_array_find(ysw_editor->music->beats, ysw_editor->beat);
        zm_beat_x beat_count = ysw_array_get_count(ysw_editor->music->beats);
        zm_beat_x next = (previous + 1) % beat_count;
        ysw_editor->beat = ysw_array_get(ysw_editor->music->beats, next);
    }
    if (division) {
        division->rhythm.beat = ysw_editor->beat;
        ysw_staff_invalidate(ysw_editor->staff);
    }
    display_mode(ysw_editor);
    play_position(ysw_editor);
}

static zm_division_t *apply_note(ysw_editor_t *ysw_editor, zm_note_t midi_note, zm_time_x duration_millis)
{
    zm_time_x articulation = 0;
    zm_duration_t duration = ysw_editor->duration;
    if (duration == ZM_AS_PLAYED) {
        zm_bpm_x bpm = zm_tempo_to_bpm(ysw_editor->pattern->tempo);
        duration = ysw_millis_to_ticks(duration_millis, bpm);
        articulation = ysw_millis_to_ticks(ysw_editor->delta, bpm);
    }

    zm_division_t *division = NULL;
    int32_t division_index = ysw_editor->position / 2;
    bool is_new_division = is_space_position(ysw_editor);
    if (is_new_division) {
        if (articulation && division_index) {
            zm_division_t *rest = ysw_heap_allocate(sizeof(zm_division_t));
            rest->melody.duration = min(articulation, ZM_WHOLE);
            ysw_array_insert(ysw_editor->pattern->divisions, division_index, rest);
            division_index++;
            ysw_editor->position = division_index * 2 + 1;
        }
        division = ysw_heap_allocate(sizeof(zm_division_t));
        ysw_array_insert(ysw_editor->pattern->divisions, division_index, division);
    } else {
        division = ysw_array_get(ysw_editor->pattern->divisions, division_index);
    }

    if (ysw_editor->mode == YSW_EDITOR_MODE_MELODY) {
        division->melody.note = midi_note; // rest == 0
        division->melody.duration = duration;
    } else if (ysw_editor->mode == YSW_EDITOR_MODE_CHORD) {
        division->chord.root = midi_note; // rest == 0
        division->chord.quality = ysw_editor->quality;
        division->chord.style = ysw_editor->style;
        division->chord.duration = 0; // set by recalculate
        if (!division->melody.duration) {
            division->melody.duration = duration;
        }
    } else if (ysw_editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        division->rhythm.surface = midi_note; // rest == 0
        if (!midi_note) {
            division->rhythm.beat = NULL;
        }
        if (!division->melody.duration) {
            division->melody.duration = duration;
        }
    }

    zm_division_x division_count = ysw_array_get_count(ysw_editor->pattern->divisions);
    ysw_editor->position = min(ysw_editor->position + ysw_editor->advance, division_count * 2);
    ysw_staff_set_position(ysw_editor->staff, ysw_editor->position);
    display_mode(ysw_editor);
    recalculate(ysw_editor);
    return division;
}

static void insert_beat(ysw_editor_t *ysw_editor)
{
    uint32_t last_start = 0;
    assert(ysw_editor->beat);
    if (!is_space_position(ysw_editor)) {
        ysw_editor->position++;
    }
    zm_duration_t saved_duration = ysw_editor->duration;
    zm_beat_x stroke_count = ysw_array_get_count(ysw_editor->beat->strokes);
    for (zm_beat_x i = 0; i <= stroke_count; i++) {
        zm_time_x start;
        if (i < stroke_count) {
            zm_stroke_t *stroke = ysw_array_get(ysw_editor->beat->strokes, i);
            start = stroke->start;
        } else {
            start = zm_get_ticks_per_measure(ysw_editor->pattern->time);
        }
        if (start != last_start) {
            ysw_editor->duration = zm_round_duration(start - last_start, NULL, NULL);
            zm_division_t *division = apply_note(ysw_editor, 0, 0);
            if (!last_start) {
                division->rhythm.beat = ysw_editor->beat;
            }
            last_start = start;
        }
    }
    ysw_editor->duration = saved_duration;
}

static void process_delete(ysw_editor_t *ysw_editor)
{
    zm_division_x division_index = ysw_editor->position / 2;
    zm_division_x division_count = ysw_array_get_count(ysw_editor->pattern->divisions);
    if (division_count > 0 && division_index == division_count) {
        // On space following last division in pattern, delete previous division
        division_index--;
    }
    if (division_index < division_count) {
        zm_division_t *division = ysw_array_remove(ysw_editor->pattern->divisions, division_index);
        ysw_heap_free(division);
        division_count--;
        if (division_index == division_count) {
            if (division_count) {
                ysw_editor->position -= 2;
            } else {
                ysw_editor->position = 0;
            }
        }
        ysw_staff_set_position(ysw_editor->staff, ysw_editor->position);
        recalculate(ysw_editor);
        display_mode(ysw_editor);
    }
}

static void process_left(ysw_editor_t *ysw_editor, uint8_t move_amount)
{
    ESP_LOGD(TAG, "process_left");
    if (ysw_editor->position >= move_amount) {
        ysw_editor->position -= move_amount;
        play_position(ysw_editor);
        ysw_staff_set_position(ysw_editor->staff, ysw_editor->position);
        display_mode(ysw_editor);
    }
}

static void process_right(ysw_editor_t *ysw_editor, uint8_t move_amount)
{
    uint32_t new_position = ysw_editor->position + move_amount;
    if (new_position <= ysw_array_get_count(ysw_editor->pattern->divisions) * 2) {
        ysw_editor->position = new_position;
        play_position(ysw_editor);
        ysw_staff_set_position(ysw_editor->staff, ysw_editor->position);
        display_mode(ysw_editor);
    }
}

static void process_up(ysw_editor_t *ysw_editor)
{
    if (is_division_position(ysw_editor)) {
        zm_division_x division_index = ysw_editor->position / 2;
        zm_division_t *division = ysw_array_get(ysw_editor->pattern->divisions, division_index);
        if (ysw_editor->mode == YSW_EDITOR_MODE_MELODY) {
            if (division->melody.note && division->melody.note < 83) {
                division->melody.note++;
            }
        } else if (ysw_editor->mode == YSW_EDITOR_MODE_CHORD) {
            if (division->chord.root && division->chord.root < 83) {
                division->chord.root++;
            }
        } else if (ysw_editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        }
        play_position(ysw_editor);
        recalculate(ysw_editor);
        display_mode(ysw_editor);
    }
}

static void process_down(ysw_editor_t *ysw_editor)
{
    if (is_division_position(ysw_editor)) {
        zm_division_x division_index = ysw_editor->position / 2;
        zm_division_t *division = ysw_array_get(ysw_editor->pattern->divisions, division_index);
        if (ysw_editor->mode == YSW_EDITOR_MODE_MELODY) {
            if (division->melody.note && division->melody.note > 60) {
                division->melody.note--;
            }
        } else if (ysw_editor->mode == YSW_EDITOR_MODE_CHORD) {
            if (division->chord.root && division->chord.root > 60) {
                division->chord.root--;
            }
        } else if (ysw_editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        }
        play_position(ysw_editor);
        recalculate(ysw_editor);
        display_mode(ysw_editor);
    }
}

static void on_melody(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    ysw_editor->mode = YSW_EDITOR_MODE_MELODY;
    display_mode(ysw_editor);
}

static void on_chord(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    ysw_editor->mode = YSW_EDITOR_MODE_CHORD;
    display_mode(ysw_editor);
}

static void on_rhythm(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    ysw_editor->mode = YSW_EDITOR_MODE_RHYTHM;
    display_mode(ysw_editor);
}

static void on_program(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    cycle_program(ysw_editor);
}

static void on_quality(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    cycle_quality(ysw_editor);
}

static void on_style(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    cycle_style(ysw_editor);
}

static void on_key_signature(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    cycle_key_signature(ysw_editor);
}

static void on_time_signature(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    cycle_time_signature(ysw_editor);
}

static void on_tempo(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    cycle_tempo(ysw_editor);
}

static void on_duration(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    cycle_duration(ysw_editor);
}

static void on_tie(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    cycle_tie(ysw_editor);
}

static void on_cycle_beat(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    cycle_beat(ysw_editor);
}

static void on_insert_beat(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    insert_beat(ysw_editor);
}

static void on_delete(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    process_delete(ysw_editor);
}

static void on_play(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    ysw_array_t *notes = zm_render_pattern(ysw_editor->music, ysw_editor->pattern, BACKGROUND_BASE);
    zm_bpm_x bpm = zm_tempo_to_bpm(ysw_editor->pattern->tempo);
    ysw_event_fire_play(ysw_editor->bus, notes, bpm);
}

static void on_demo(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    zm_composition_t *composition = ysw_array_get(ysw_editor->music->compositions, 0);
    ysw_array_t *notes = zm_render_composition(ysw_editor->music, composition, BACKGROUND_BASE);
    ysw_event_fire_play(ysw_editor->bus, notes, composition->bpm);
}

static void on_stop(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    ysw_event_fire_stop(ysw_editor->bus);
}

static void on_loop(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    ysw_editor->loop = !ysw_editor->loop;
    ysw_event_fire_loop(ysw_editor->bus, ysw_editor->loop);
}

static void on_left(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ESP_LOGD(TAG, "on_left");
    ysw_editor_t *ysw_editor = menu->context;
    process_left(ysw_editor, 1);
}

static void on_right(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    process_right(ysw_editor, 1);
}

static void on_previous(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    process_left(ysw_editor, 2);
}

static void on_next(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    process_right(ysw_editor, 2);
}

static void on_up(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    process_up(ysw_editor);
}

static void on_down(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    process_down(ysw_editor);
}

static void fire_note_on(ysw_editor_t *ysw_editor, zm_note_t midi_note, zm_channel_x channel)
{
    ysw_event_note_on_t note_on = {
        .channel = channel,
        .midi_note = midi_note,
        .velocity = 80,
    };
    ysw_event_fire_note_on(ysw_editor->bus, YSW_ORIGIN_EDITOR, &note_on);
}

static void fire_note_off(ysw_editor_t *ysw_editor, zm_note_t midi_note, zm_channel_x channel)
{
    ysw_event_note_off_t note_off = {
        .channel = channel,
        .midi_note = midi_note,
    };
    ysw_event_fire_note_off(ysw_editor->bus, YSW_ORIGIN_EDITOR, &note_off);
}

static void on_note(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    zm_note_t midi_note = (uintptr_t)value;
    if (event->header.type == YSW_EVENT_KEY_DOWN) {
        if (ysw_editor->mode == YSW_EDITOR_MODE_MELODY) {
            ysw_editor->down_at = event->key_down.time;
            ysw_editor->delta = ysw_editor->down_at - ysw_editor->up_at;
            if (midi_note) {
                fire_note_on(ysw_editor, midi_note, MELODY_CHANNEL);
            }
        } else if (ysw_editor->mode == YSW_EDITOR_MODE_CHORD) {
            zm_division_t division = {
                .chord.root = midi_note,
                .chord.quality = ysw_editor->quality,
                .chord.style = ysw_editor->style,
                .chord.duration = zm_get_ticks_per_measure(ysw_editor->pattern->time),
            };
            play_division(ysw_editor, &division);
        } else if (ysw_editor->mode == YSW_EDITOR_MODE_RHYTHM) {
            if (midi_note) {
                fire_note_on(ysw_editor, midi_note, RHYTHM_CHANNEL);
            }
        }
    } else if (event->header.type == YSW_EVENT_KEY_UP) {
        if (ysw_editor->mode == YSW_EDITOR_MODE_MELODY) {
            ysw_editor->up_at = event->key_up.time;
            if (midi_note) {
                fire_note_off(ysw_editor, midi_note, MELODY_CHANNEL);
            }
        } else if (ysw_editor->mode == YSW_EDITOR_MODE_RHYTHM) {
            if (midi_note) {
                fire_note_off(ysw_editor, midi_note, RHYTHM_CHANNEL);
            }
        }
        apply_note(ysw_editor, midi_note, event->key_pressed.duration);
    }
}

int compare_divisions(const void *left, const void *right)
{
    const zm_division_t *left_division = *(zm_division_t * const *)left;
    const zm_division_t *right_division = *(zm_division_t * const *)right;
    int delta = left_division->start - right_division->start;
    return delta;
}

static void on_note_status(ysw_editor_t *ysw_editor, ysw_event_t *event)
{
    if (event->note_status.note.channel >= BACKGROUND_BASE) {
        zm_division_t needle = {
            .start = event->note_status.note.start,
        };
        ysw_array_match_t flags = YSW_ARRAY_MATCH_EXACT;
        int32_t result = ysw_array_search(ysw_editor->pattern->divisions, &needle, compare_divisions, flags);
        if (result != -1) {
            ysw_editor->position = division_index_to_position(result);
            ysw_staff_set_position(ysw_editor->staff, ysw_editor->position);
        } else {
            // e.g. a stroke in a beat
        }
    }
}

static void on_new(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    // TODO: see if current pattern is modified, and if so, prompt to save or cancel
    zm_pattern_t *pattern = zm_music_create_pattern(ysw_editor->music);
    ysw_event_fire_pattern_edit(ysw_editor->bus, pattern);
}

static void on_save(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    // TODO: see if current pattern has been added to music->patterns, and if not, add it
    zm_save_music(ysw_editor->music);
}

static void on_note_length(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *ysw_editor = menu->context;
    int32_t direction = (intptr_t)value;
    ESP_LOGD(TAG, "direction=%d", direction);
    zm_division_t *division = get_division(ysw_editor);
    if (division) {
        division->melody.duration = zm_get_next_dotted_duration(division->melody.duration, direction);
        play_position(ysw_editor);
        recalculate(ysw_editor);
        display_mode(ysw_editor);
    }
}

static void on_pattern_edit(ysw_editor_t *ysw_editor, ysw_event_t *event)
{
    ysw_editor->position = 0;
    ysw_editor->pattern = event->pattern_edit.pattern;
    ysw_staff_set_pattern(ysw_editor->staff, ysw_editor->pattern);
    ysw_footer_set_key(ysw_editor->footer, ysw_editor->pattern->key);
    ysw_footer_set_time(ysw_editor->footer, ysw_editor->pattern->time);
    ysw_footer_set_tempo(ysw_editor->footer, ysw_editor->pattern->tempo);
    fire_program_change(ysw_editor, ysw_editor->pattern->melody_program, MELODY_CHANNEL);
    fire_program_change(ysw_editor, ysw_editor->pattern->chord_program, CHORD_CHANNEL);
    fire_program_change(ysw_editor, ysw_editor->pattern->rhythm_program, RHYTHM_CHANNEL);
    display_mode(ysw_editor); // mode displays program
}

static void process_event_with_pattern(ysw_editor_t *ysw_editor, ysw_event_t *event)
{
    switch (event->header.type) {
        case YSW_EVENT_KEY_DOWN:
            ysw_menu_on_key_down(ysw_editor->menu, event);
            break;
        case YSW_EVENT_KEY_UP:
            ysw_menu_on_key_up(ysw_editor->menu, event);
            break;
        case YSW_EVENT_KEY_PRESSED:
            ysw_menu_on_key_pressed(ysw_editor->menu, event);
            break;
        case YSW_EVENT_NOTE_STATUS:
            on_note_status(ysw_editor, event);
            break;
        case YSW_EVENT_PATTERN_EDIT:
            on_pattern_edit(ysw_editor, event);
            break;
        default:
            break;
    }
}

static void process_event_without_pattern(ysw_editor_t *ysw_editor, ysw_event_t *event)
{
    switch (event->header.type) {
        case YSW_EVENT_PATTERN_EDIT:
            on_pattern_edit(ysw_editor, event);
            break;
        default:
            break;
    }
}

static void process_event(void *context, ysw_event_t *event)
{
    ysw_editor_t *ysw_editor = context;
    if (event) {
        if (ysw_editor->pattern) {
            process_event_with_pattern(ysw_editor, event);
        } else {
            process_event_without_pattern(ysw_editor, event);
        }
    }
    lv_task_handler();
}

static void initialize_editor_task(void *context)
{
    ysw_editor_t *ysw_editor = context;

    ysw_editor->lvgl_init();
    ysw_editor->container = lv_obj_create(lv_scr_act(), NULL);
    assert(ysw_editor->container);

    // See https://docs.lvgl.io/v7/en/html/overview/style.html
    lv_obj_set_style_local_bg_color(ysw_editor->container, 0, 0, LV_COLOR_MAROON);
    lv_obj_set_style_local_bg_grad_color(ysw_editor->container, 0, 0, LV_COLOR_BLACK);
    lv_obj_set_style_local_bg_grad_dir(ysw_editor->container, 0, 0, LV_GRAD_DIR_VER);
    lv_obj_set_style_local_bg_opa(ysw_editor->container, 0, 0, LV_OPA_100);

    lv_obj_set_style_local_text_color(ysw_editor->container, 0, 0, LV_COLOR_YELLOW);
    lv_obj_set_style_local_text_opa(ysw_editor->container, 0, 0, LV_OPA_100);

    lv_obj_set_size(ysw_editor->container, 320, 240);
    lv_obj_align(ysw_editor->container, NULL, LV_ALIGN_CENTER, 0, 0);

    ysw_editor->header = ysw_header_create(ysw_editor->container);
    lv_obj_set_size(ysw_editor->header, 320, 30);
    lv_obj_align(ysw_editor->header, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

    ysw_editor->staff = ysw_staff_create(ysw_editor->container);
    assert(ysw_editor->staff);
    lv_obj_set_size(ysw_editor->staff, 320, 180);
    lv_obj_align(ysw_editor->staff, NULL, LV_ALIGN_CENTER, 0, 0);

    ysw_editor->footer = ysw_footer_create(ysw_editor->container);
    lv_obj_set_size(ysw_editor->footer, 320, 30);
    lv_obj_align(ysw_editor->footer, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    ysw_footer_set_duration(ysw_editor->footer, ysw_editor->duration);
}

// Layout of keycodes generated by keyboard:
//
//    0,  1,      2,  3,  4,      5,  6,  7,  8,
//  9, 10, 11, 12, 13, 14, 15,   16, 17, 18, 19,
//   20, 21,     22, 23, 24,     25, 26, 27, 28,
// 29, 30, 31, 32, 33, 34, 35,   36, 37, 38, 39,

static const ysw_menu_softmap_t softmap[] = {
    5, 6, 7, 8, YSW_MENU_ENDLINE,
    16, 17, 18, 19, YSW_MENU_ENDLINE,
    25, 26, 27, 28, YSW_MENU_ENDLINE,
    36, 37, 38, 39, YSW_MENU_ENDGRID,
};

#define VP (void *)(uintptr_t)

#define YSW_MF_BUTTON (YSW_MENU_DOWN|YSW_MENU_UP)
#define YSW_MF_COMMAND (YSW_MENU_PRESS|YSW_MENU_CLOSE)
#define YSW_MF_BUTTON_COMMAND (YSW_MENU_DOWN|YSW_MENU_UP|YSW_MENU_CLOSE)
#define YSW_MF_PLUS (YSW_MENU_UP|YSW_MENU_OPEN)
#define YSW_MF_MINUS (YSW_MENU_UP|YSW_MENU_POP)
#define YSW_MF_NOP (0)

static const ysw_menu_item_t menu_2[] = {
    { 5, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 6, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 7, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 16, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 17, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 18, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 25, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 26, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 27, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 36, " ", YSW_MF_PLUS, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, NULL },
};

static const ysw_menu_item_t melody_menu[] = {
    { 5, "Tie\nNotes", YSW_MF_COMMAND, on_tie, 0 },
    { 6, "Insert\nRest", YSW_MF_COMMAND, on_note, 0 },
    { 7, "Melody\nSound", YSW_MF_COMMAND, on_program, 0 },

    { 16, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 17, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 18, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 25, "Note\nLength-", YSW_MF_COMMAND, on_note_length, VP -1 },
    { 26, "Note\nLength+", YSW_MF_COMMAND, on_note_length, VP +1 },
    { 27, "Clear\nNote", YSW_MF_COMMAND, ysw_menu_nop, 0 },

    { 36, " ", YSW_MF_PLUS, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, NULL },
};

static const ysw_menu_item_t chord_menu[] = {
    { 5, "Chord\nQuality", YSW_MF_COMMAND, on_quality, 0 },
    { 6, "Chord\nStyle", YSW_MF_COMMAND, on_style, 0 },
    { 7, "Chord\nSound", YSW_MF_COMMAND, on_program, 0 },

    { 16, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 17, "Apply\nQ&S", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 18, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 25, " ", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 26, " ", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 27, "Clear\nChord", YSW_MF_COMMAND, ysw_menu_nop, 0 },

    { 36, " ", YSW_MF_PLUS, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, NULL },
};

static const ysw_menu_item_t rhythm_menu[] = {
    { 5, "Cycle\nBeat", YSW_MF_COMMAND, on_cycle_beat, 0 },
    { 6, "Insert\nBeat", YSW_MF_COMMAND, on_insert_beat, 0 },
    { 7, "Rhythm\nSound", YSW_MF_COMMAND, on_program, 0 },

    { 16, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 17, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 18, "Clear\nStroke", YSW_MF_COMMAND, ysw_menu_nop, 0 },

    { 25, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 26, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 27, "Clear\nBeat", YSW_MF_COMMAND, ysw_menu_nop, 0 },

    { 36, " ", YSW_MF_PLUS, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, NULL },
};

static const ysw_menu_item_t file_menu[] = {
    { 5, "New", YSW_MF_COMMAND, on_new, 0 },
    { 6, "Save", YSW_MF_COMMAND, on_save, 0 },
    { 7, "Save As", YSW_MF_COMMAND, ysw_menu_nop, 0 },

    { 16, "Open", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 17, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 18, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 25, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 26, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 27, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 36, " ", YSW_MF_PLUS, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, NULL },
};

static const ysw_menu_item_t edit_menu[] = {
    { 5, "Select\nLeft", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 6, "Select\nRight", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 7, "Select\nAll", YSW_MF_COMMAND, ysw_menu_nop, 0 },

    { 16, "Copy", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 17, "Paste", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 18, "Cut", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 25, "Previous", YSW_MF_COMMAND, on_previous, 0 },
    { 26, "Next", YSW_MF_COMMAND, on_next, 0 },
    { 27, "Delete", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 36, " ", YSW_MF_PLUS, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, NULL },
};

static const ysw_menu_item_t defs_menu[] = {
    { 5, "Sounds", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 6, "Qualities", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 7, "Styles", YSW_MF_COMMAND, ysw_menu_nop, 0 },

    { 16, "Beats", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 17, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 18, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 25, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 26, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 27, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 36, " ", YSW_MF_PLUS, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, NULL },
};

static const ysw_menu_item_t listen_menu[] = {
    { 5, "Play", YSW_MF_COMMAND, on_play, 0 },
    { 6, "Pause", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 7, "Resume", YSW_MF_COMMAND, ysw_menu_nop, 0 },

    { 16, "Stop", YSW_MF_COMMAND, on_stop, 0 },
    { 17, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 18, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 25, "Loop", YSW_MF_COMMAND, on_loop, 0 },
    { 26, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 27, "Cycle\nDemo", YSW_MF_COMMAND, on_demo, 0 },

    { 36, " ", YSW_MF_PLUS, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, NULL },
};

static const ysw_menu_item_t settings_menu[] = {
    { 5, "Key\nSig", YSW_MF_COMMAND, on_key_signature, 0 },
    { 6, "Time\nSig", YSW_MF_COMMAND, on_time_signature, 0 },
    { 7, "Tempo\n(BPM)", YSW_MF_COMMAND, on_tempo, 0 },

    { 16, "Note\nDuration", YSW_MF_COMMAND, on_duration, 0 },
    { 17, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 18, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 25, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 26, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 27, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 36, " ", YSW_MF_PLUS, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, NULL },
};

static const ysw_menu_item_t preferences_menu[] = {
    { 5, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 6, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 7, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 16, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 17, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 18, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 25, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 26, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 27, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 36, " ", YSW_MF_PLUS, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, NULL },
};

static const ysw_menu_item_t main_menu[] = {
    { 0, "C#6", YSW_MF_BUTTON, on_note, VP 73 },
    { 1, "D#6", YSW_MF_BUTTON, on_note, VP 75 },
    { 2, "F#6", YSW_MF_BUTTON, on_note, VP 78 },
    { 3, "G#6", YSW_MF_BUTTON, on_note, VP 80 },
    { 4, "A#6", YSW_MF_BUTTON, on_note, VP 82 },

    { 5, "Melody", YSW_MF_PLUS, on_melody, (void*)melody_menu },
    { 6, "Chord", YSW_MF_PLUS, on_chord, (void*)chord_menu },
    { 7, "Rhythm", YSW_MF_PLUS, on_rhythm, (void*)rhythm_menu },
    { 8, "Up", YSW_MF_COMMAND, on_up, 0 },

    { 9, "C6", YSW_MF_BUTTON, on_note, VP 72 },
    { 10, "D6", YSW_MF_BUTTON, on_note, VP 74 },
    { 11, "E6", YSW_MF_BUTTON, on_note, VP 76 },
    { 12, "F6", YSW_MF_BUTTON, on_note, VP 77 },
    { 13, "G6", YSW_MF_BUTTON, on_note, VP 79 },
    { 14, "A6", YSW_MF_BUTTON, on_note, VP 81 },
    { 15, "B6", YSW_MF_BUTTON, on_note, VP 83 },

    { 16, "File", YSW_MF_PLUS, ysw_menu_nop, (void*)file_menu },
    { 17, "Edit", YSW_MF_PLUS, ysw_menu_nop, (void*)edit_menu },
    { 18, "Music\nDefs", YSW_MF_PLUS, ysw_menu_nop, (void*)defs_menu },
    { 19, "Down", YSW_MF_COMMAND, on_down, 0 },

    { 20, "C#5", YSW_MF_BUTTON, on_note, VP 61 },
    { 21, "D#5", YSW_MF_BUTTON, on_note, VP 63 },
    { 22, "F#5", YSW_MF_BUTTON, on_note, VP 66 },
    { 23, "G#5", YSW_MF_BUTTON, on_note, VP 68 },
    { 24, "A#5", YSW_MF_BUTTON, on_note, VP 70 },

    { 25, "Listen", YSW_MF_PLUS, ysw_menu_nop, (void*)listen_menu },
    { 26, "Song\nSettings", YSW_MF_PLUS, ysw_menu_nop, (void*)settings_menu },
    { 27, "Prefs", YSW_MF_PLUS, ysw_menu_nop, (void*)preferences_menu },
    { 28, "Left", YSW_MF_COMMAND, on_left, 0 },

    { 29, "C5", YSW_MF_BUTTON, on_note, VP 60 },
    { 30, "D5", YSW_MF_BUTTON, on_note, VP 62 },
    { 31, "E5", YSW_MF_BUTTON, on_note, VP 64 },
    { 32, "F5", YSW_MF_BUTTON, on_note, VP 65 },
    { 33, "G5", YSW_MF_BUTTON, on_note, VP 67 },
    { 34, "A5", YSW_MF_BUTTON, on_note, VP 69 },
    { 35, "B5", YSW_MF_BUTTON, on_note, VP 71 },

    { 36, "Menu+", YSW_MF_PLUS, ysw_menu_nop, (void*)menu_2 },
    { 37, "Delete", YSW_MF_COMMAND, on_delete, 0 },
    { 38, "Menu-", YSW_MF_MINUS, ysw_menu_nop, 0 },
    { 39, "Right", YSW_MF_COMMAND, on_right, 0 },

    { 0, NULL, 0, NULL, NULL },
};

void ysw_editor_create_task(ysw_bus_t *bus, zm_music_t *music, ysw_editor_lvgl_init lvgl_init)
{
    assert(bus);
    assert(music);
    assert(lvgl_init);

    assert(ysw_array_get_count(music->programs));
    assert(ysw_array_get_count(music->qualities) > DEFAULT_QUALITY);
    assert(ysw_array_get_count(music->styles));

    ysw_editor_t *ysw_editor = ysw_heap_allocate(sizeof(ysw_editor_t));

    ysw_editor->bus = bus;
    ysw_editor->lvgl_init = lvgl_init;
    ysw_editor->music = music;

    ysw_editor->quality = ysw_array_get(music->qualities, DEFAULT_QUALITY);
    ysw_editor->style = ysw_array_get(music->styles, DEFAULT_STYLE);
    ysw_editor->beat = ysw_array_get(music->beats, DEFAULT_BEAT);
    ysw_editor->duration = ZM_AS_PLAYED;

    ysw_editor->advance = 2;
    ysw_editor->position = 0;
    ysw_editor->mode = YSW_EDITOR_MODE_MELODY;

    ysw_editor->menu = ysw_menu_create(main_menu, softmap, ysw_editor);

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.initializer = initialize_editor_task;
    config.event_handler = process_event;
    config.context = ysw_editor;
    config.wait_millis = 5;
    //config.priority = YSW_TASK_DEFAULT_PRIORITY + 1;
    //config.priority = configMAX_PRIORITIES - 1;
    config.priority = 24;

    ysw_task_t *task = ysw_task_create(&config);

    ysw_task_subscribe(task, YSW_ORIGIN_FILE);
    ysw_task_subscribe(task, YSW_ORIGIN_KEYBOARD);
    ysw_task_subscribe(task, YSW_ORIGIN_SEQUENCER);
}

