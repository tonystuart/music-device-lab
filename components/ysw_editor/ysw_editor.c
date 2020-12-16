// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_editor.h"
#include "ysw_chooser.h"
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
    ysw_chooser_t *chooser;

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

static inline bool is_division_position(ysw_editor_t *editor)
{
    return editor->position % 2 == 1;
}

static inline bool is_space_position(ysw_editor_t *editor)
{
    return editor->position % 2 == 0;
}
 
static inline uint32_t division_index_to_position(zm_division_x division_index)
{
    return (division_index * 2) + 1;
}

static zm_division_t *get_division(ysw_editor_t *editor)
{
    zm_division_t *division = NULL;
    if (is_division_position(editor)) {
        zm_division_x division_index = editor->position / 2;
        division = ysw_array_get(editor->pattern->divisions, division_index);
    }
    return division;
}

// TODO: remove unnecessary calls to recalculate (e.g. increase/decrease pitch)

static void recalculate(ysw_editor_t *editor)
{
    zm_time_x start = 0;
    zm_measure_x measure = 1;
    uint32_t ticks_in_measure = 0;

    zm_division_t *division = NULL;
    zm_division_t *previous = NULL;

    zm_time_x ticks_per_measure = zm_get_ticks_per_measure(editor->pattern->time);
    zm_division_x division_count = ysw_array_get_count(editor->pattern->divisions);

    for (zm_division_x i = 0; i < division_count; i++) {
        division = ysw_array_get(editor->pattern->divisions, i);
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

    ysw_staff_invalidate(editor->staff);
}

static void play_division(ysw_editor_t *editor, zm_division_t *division)
{
    ysw_array_t *notes = zm_render_division(editor->music, editor->pattern, division, BASE_CHANNEL);
    zm_bpm_x bpm = zm_tempo_to_bpm(editor->pattern->tempo);
    ysw_event_fire_play(editor->bus, notes, bpm);
}

static void play_position(ysw_editor_t *editor)
{
    if (is_division_position(editor)) {
        zm_division_x division_index = editor->position / 2;
        zm_division_t *division = ysw_array_get(editor->pattern->divisions, division_index);
        play_division(editor, division);
    }
}

static void display_program(ysw_editor_t *editor)
{
    const char *value = "";
    if (editor->mode == YSW_EDITOR_MODE_MELODY) {
        value = editor->pattern->melody_program->name;
    } else if (editor->mode == YSW_EDITOR_MODE_CHORD) {
        value = editor->pattern->chord_program->name;
    } else if (editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        value = editor->pattern->rhythm_program->name;
    }
    ysw_header_set_program(editor->header, value);
}

static void display_melody_mode(ysw_editor_t *editor)
{
    // 1. on a division - show note or rest
    // 2. between divisions - show previous (to left) note or rest
    // 3. no divisions - show blank
    char value[32];
    zm_division_x division_count = ysw_array_get_count(editor->pattern->divisions);
    if (editor->position > 0) {
        zm_division_x division_index = editor->position / 2;
        if (division_index >= division_count) {
            division_index = division_count - 1;
        }
        zm_bpm_x bpm = zm_tempo_to_bpm(editor->pattern->tempo);
        zm_division_t *division = ysw_array_get(editor->pattern->divisions, division_index);
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
        snprintf(value, sizeof(value), "%s", editor->pattern->name);
    }
    ysw_header_set_mode(editor->header, modes[editor->mode], value);
}

static void display_chord_mode(ysw_editor_t *editor)
{
    // 1. on a division with a chord value - show chord value, quality, style
    // 2. on a division without a chord value - show quality, style
    // 3. between divisions - show quality, style
    // 4. no divisions - show quality, style
    char value[32] = {};
    zm_division_t *division = NULL;
    if (is_division_position(editor)) {
        division = ysw_array_get(editor->pattern->divisions, editor->position / 2);
    }
    if (division && division->chord.root) {
        snprintf(value, sizeof(value), "%s %s %s",
                zm_get_note_name(division->chord.root),
                division->chord.quality->name,
                division->chord.style->name);
    } else {
        snprintf(value, sizeof(value), "%s %s",
                editor->quality->name,
                editor->style->name);
    }
    ysw_header_set_mode(editor->header, modes[editor->mode], value);
}

static void display_rhythm_mode(ysw_editor_t *editor)
{
    char value[32] = {};
    zm_division_t *division = NULL;
    if (is_division_position(editor)) {
        division = ysw_array_get(editor->pattern->divisions, editor->position / 2);
    }
    if (division && division->rhythm.beat) {
        snprintf(value, sizeof(value), "%s", division->rhythm.beat->name);
    } else {
        snprintf(value, sizeof(value), "%s", editor->beat->name);
    }
    ysw_header_set_mode(editor->header, modes[editor->mode], value);
}

static void display_mode(ysw_editor_t *editor)
{
    // program is mode specific -- update it first so mode resizing takes precedence
    display_program(editor);
    switch (editor->mode) {
        case YSW_EDITOR_MODE_MELODY:
            display_melody_mode(editor);
            break;
        case YSW_EDITOR_MODE_CHORD:
            display_chord_mode(editor);
            break;
        case YSW_EDITOR_MODE_RHYTHM:
            display_rhythm_mode(editor);
            break;
    }
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

static void fire_program_change(ysw_editor_t *editor, zm_program_t *program, zm_channel_x channel)
{
    ysw_event_program_change_t program_change = {
        .channel = channel,
        .program = ysw_array_find(editor->music->programs, program),
    };
    ysw_event_fire_program_change(editor->bus, YSW_ORIGIN_EDITOR, &program_change);
}

static void cycle_program(ysw_editor_t *editor)
{
    if (editor->mode == YSW_EDITOR_MODE_MELODY) {
        editor->pattern->melody_program = next_program(editor->music->programs,
                editor->pattern->melody_program, ZM_PROGRAM_NOTE);
        fire_program_change(editor, editor->pattern->melody_program, MELODY_CHANNEL);
    } else if (editor->mode == YSW_EDITOR_MODE_CHORD) {
        editor->pattern->chord_program = next_program(editor->music->programs,
                editor->pattern->chord_program, ZM_PROGRAM_NOTE);
        fire_program_change(editor, editor->pattern->chord_program, CHORD_CHANNEL);
    } else if (editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        editor->pattern->rhythm_program = next_program(editor->music->programs,
                editor->pattern->rhythm_program, ZM_PROGRAM_BEAT);
        fire_program_change(editor, editor->pattern->rhythm_program, RHYTHM_CHANNEL);
    }
    display_program(editor);
    play_position(editor);
}

static void cycle_key_signature(ysw_editor_t *editor)
{
    editor->pattern->key = zm_get_next_key_index(editor->pattern->key);
    recalculate(editor);
    ysw_footer_set_key(editor->footer, editor->pattern->key);
}

static void cycle_time_signature(ysw_editor_t *editor)
{
    editor->pattern->time = zm_get_next_time_index(editor->pattern->time);
    recalculate(editor);
    ysw_footer_set_time(editor->footer, editor->pattern->time);
}

static void cycle_tempo(ysw_editor_t *editor)
{
    editor->pattern->tempo = zm_get_next_tempo_index(editor->pattern->tempo);
    recalculate(editor);
    ysw_footer_set_tempo(editor->footer, editor->pattern->tempo);
    display_mode(editor);
}

static void cycle_duration(ysw_editor_t *editor)
{
    zm_division_t *division = NULL;
    if (is_division_position(editor) && editor->duration != ZM_AS_PLAYED) {
        zm_division_x division_index = editor->position / 2;
        division = ysw_array_get(editor->pattern->divisions, division_index);
    }
    // Cycle default duration if not on a division or if division duration is already default duration
    if (!division || division->melody.duration == editor->duration) {
        editor->duration = zm_get_next_duration(editor->duration);
        ysw_footer_set_duration(editor->footer, editor->duration);
    }
    if (division) {
        division->melody.duration = editor->duration;
        recalculate(editor);
        display_mode(editor);
    }
}

static void cycle_tie(ysw_editor_t *editor)
{
    if (is_division_position(editor)) {
        zm_division_x division_index = editor->position / 2;
        zm_division_t *division = ysw_array_get(editor->pattern->divisions, division_index);
        if (division->melody.note) {
            if (division->melody.tie) {
                division->melody.tie = 0;
            } else {
                division->melody.tie = 1;
            }
            ysw_staff_invalidate(editor->staff);
        }
    }
}

static zm_style_t *find_matching_style(ysw_editor_t *editor, bool preincrement)
{

    zm_style_x current = ysw_array_find(editor->music->styles, editor->style);
    zm_style_x style_count = ysw_array_get_count(editor->music->styles);
    zm_distance_x distance_count = ysw_array_get_count(editor->quality->distances);
    for (zm_style_x i = 0; i < style_count; i++) {
        if (preincrement) {
            current = (current + 1) % style_count;
        }
        zm_style_t *style = ysw_array_get(editor->music->styles, current);
        if (style->distance_count == distance_count) {
            editor->style = style;
            return style;
        }
        if (!preincrement) {
            current = (current + 1) % style_count;
        }
    }
    assert(false);
}

static void cycle_quality(ysw_editor_t *editor)
{
    zm_division_t *division = NULL;
    if (is_division_position(editor)) {
        zm_division_x division_index = editor->position / 2;
        division = ysw_array_get(editor->pattern->divisions, division_index);
    }
    if (!division || !division->chord.root || division->chord.quality == editor->quality) {
        zm_quality_x previous = ysw_array_find(editor->music->qualities, editor->quality);
        zm_quality_x quality_count = ysw_array_get_count(editor->music->qualities);
        zm_quality_x next = (previous + 1) % quality_count;
        editor->quality = ysw_array_get(editor->music->qualities, next);
        editor->style = find_matching_style(editor, false);
    }
    if (division) {
        division->chord.quality = editor->quality;
        division->chord.style = editor->style;
        ysw_staff_invalidate(editor->staff);
    }
    display_mode(editor);
    play_position(editor);
}

static void cycle_style(ysw_editor_t *editor)
{
    zm_division_t *division = NULL;
    if (is_division_position(editor)) {
        zm_division_x division_index = editor->position / 2;
        division = ysw_array_get(editor->pattern->divisions, division_index);
    }
    if (!division || !division->chord.root || division->chord.style == editor->style) {
        editor->style = find_matching_style(editor, true);
    }
    if (division) {
        division->chord.style = editor->style;
    }
    display_mode(editor);
    play_position(editor);
}

static void cycle_beat(ysw_editor_t *editor)
{
    zm_division_t *division = NULL;
    if (is_division_position(editor)) {
        zm_division_x division_index = editor->position / 2;
        division = ysw_array_get(editor->pattern->divisions, division_index);
    }
    if (!division || !division->rhythm.beat || division->rhythm.beat == editor->beat) {
        zm_beat_x previous = ysw_array_find(editor->music->beats, editor->beat);
        zm_beat_x beat_count = ysw_array_get_count(editor->music->beats);
        zm_beat_x next = (previous + 1) % beat_count;
        editor->beat = ysw_array_get(editor->music->beats, next);
    }
    if (division) {
        division->rhythm.beat = editor->beat;
        ysw_staff_invalidate(editor->staff);
    }
    display_mode(editor);
    play_position(editor);
}

static zm_division_t *apply_note(ysw_editor_t *editor, zm_note_t midi_note, zm_time_x duration_millis)
{
    zm_time_x articulation = 0;
    zm_duration_t duration = editor->duration;
    if (duration == ZM_AS_PLAYED) {
        zm_bpm_x bpm = zm_tempo_to_bpm(editor->pattern->tempo);
        duration = ysw_millis_to_ticks(duration_millis, bpm);
        articulation = ysw_millis_to_ticks(editor->delta, bpm);
    }

    zm_division_t *division = NULL;
    int32_t division_index = editor->position / 2;
    bool is_new_division = is_space_position(editor);
    if (is_new_division) {
        if (articulation && division_index) {
            zm_division_t *rest = ysw_heap_allocate(sizeof(zm_division_t));
            rest->melody.duration = min(articulation, ZM_WHOLE);
            ysw_array_insert(editor->pattern->divisions, division_index, rest);
            division_index++;
            editor->position = division_index * 2 + 1;
        }
        division = ysw_heap_allocate(sizeof(zm_division_t));
        ysw_array_insert(editor->pattern->divisions, division_index, division);
    } else {
        division = ysw_array_get(editor->pattern->divisions, division_index);
    }

    if (editor->mode == YSW_EDITOR_MODE_MELODY) {
        division->melody.note = midi_note; // rest == 0
        division->melody.duration = duration;
    } else if (editor->mode == YSW_EDITOR_MODE_CHORD) {
        division->chord.root = midi_note; // rest == 0
        division->chord.quality = editor->quality;
        division->chord.style = editor->style;
        division->chord.duration = 0; // set by recalculate
        if (!division->melody.duration) {
            division->melody.duration = duration;
        }
    } else if (editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        division->rhythm.surface = midi_note; // rest == 0
        if (!midi_note) {
            division->rhythm.beat = NULL;
        }
        if (!division->melody.duration) {
            division->melody.duration = duration;
        }
    }

    zm_division_x division_count = ysw_array_get_count(editor->pattern->divisions);
    editor->position = min(editor->position + editor->advance, division_count * 2);
    ysw_staff_set_position(editor->staff, editor->position);
    display_mode(editor);
    recalculate(editor);
    return division;
}

static void insert_beat(ysw_editor_t *editor)
{
    uint32_t last_start = 0;
    assert(editor->beat);
    if (!is_space_position(editor)) {
        editor->position++;
    }
    zm_duration_t saved_duration = editor->duration;
    zm_beat_x stroke_count = ysw_array_get_count(editor->beat->strokes);
    for (zm_beat_x i = 0; i <= stroke_count; i++) {
        zm_time_x start;
        if (i < stroke_count) {
            zm_stroke_t *stroke = ysw_array_get(editor->beat->strokes, i);
            start = stroke->start;
        } else {
            start = zm_get_ticks_per_measure(editor->pattern->time);
        }
        if (start != last_start) {
            editor->duration = zm_round_duration(start - last_start, NULL, NULL);
            zm_division_t *division = apply_note(editor, 0, 0);
            if (!last_start) {
                division->rhythm.beat = editor->beat;
            }
            last_start = start;
        }
    }
    editor->duration = saved_duration;
}

static void process_delete(ysw_editor_t *editor)
{
    zm_division_x division_index = editor->position / 2;
    zm_division_x division_count = ysw_array_get_count(editor->pattern->divisions);
    if (division_count > 0 && division_index == division_count) {
        // On space following last division in pattern, delete previous division
        division_index--;
    }
    if (division_index < division_count) {
        zm_division_t *division = ysw_array_remove(editor->pattern->divisions, division_index);
        ysw_heap_free(division);
        division_count--;
        if (division_index == division_count) {
            if (division_count) {
                editor->position -= 2;
            } else {
                editor->position = 0;
            }
        }
        ysw_staff_set_position(editor->staff, editor->position);
        recalculate(editor);
        display_mode(editor);
    }
}

static void process_left(ysw_editor_t *editor, uint8_t move_amount)
{
    ESP_LOGD(TAG, "process_left");
    if (editor->position >= move_amount) {
        editor->position -= move_amount;
        play_position(editor);
        ysw_staff_set_position(editor->staff, editor->position);
        display_mode(editor);
    }
}

static void process_right(ysw_editor_t *editor, uint8_t move_amount)
{
    uint32_t new_position = editor->position + move_amount;
    if (new_position <= ysw_array_get_count(editor->pattern->divisions) * 2) {
        editor->position = new_position;
        play_position(editor);
        ysw_staff_set_position(editor->staff, editor->position);
        display_mode(editor);
    }
}

static void process_up(ysw_editor_t *editor)
{
    if (is_division_position(editor)) {
        zm_division_x division_index = editor->position / 2;
        zm_division_t *division = ysw_array_get(editor->pattern->divisions, division_index);
        if (editor->mode == YSW_EDITOR_MODE_MELODY) {
            if (division->melody.note && division->melody.note < 83) {
                division->melody.note++;
            }
        } else if (editor->mode == YSW_EDITOR_MODE_CHORD) {
            if (division->chord.root && division->chord.root < 83) {
                division->chord.root++;
            }
        } else if (editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        }
        play_position(editor);
        recalculate(editor);
        display_mode(editor);
    }
}

static void process_down(ysw_editor_t *editor)
{
    if (is_division_position(editor)) {
        zm_division_x division_index = editor->position / 2;
        zm_division_t *division = ysw_array_get(editor->pattern->divisions, division_index);
        if (editor->mode == YSW_EDITOR_MODE_MELODY) {
            if (division->melody.note && division->melody.note > 60) {
                division->melody.note--;
            }
        } else if (editor->mode == YSW_EDITOR_MODE_CHORD) {
            if (division->chord.root && division->chord.root > 60) {
                division->chord.root--;
            }
        } else if (editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        }
        play_position(editor);
        recalculate(editor);
        display_mode(editor);
    }
}

static void on_melody(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    editor->mode = YSW_EDITOR_MODE_MELODY;
    display_mode(editor);
}

static void on_chord(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    editor->mode = YSW_EDITOR_MODE_CHORD;
    display_mode(editor);
}

static void on_rhythm(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    editor->mode = YSW_EDITOR_MODE_RHYTHM;
    display_mode(editor);
}

static void on_program(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    cycle_program(editor);
}

static void on_quality(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    cycle_quality(editor);
}

static void on_style(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    cycle_style(editor);
}

static void on_key_signature(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    cycle_key_signature(editor);
}

static void on_time_signature(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    cycle_time_signature(editor);
}

static void on_tempo(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    cycle_tempo(editor);
}

static void on_duration(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    cycle_duration(editor);
}

static void on_tie(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    cycle_tie(editor);
}

static void on_cycle_beat(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    cycle_beat(editor);
}

static void on_insert_beat(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    insert_beat(editor);
}

static void on_delete(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    process_delete(editor);
}

static void on_play(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    ysw_array_t *notes = zm_render_pattern(editor->music, editor->pattern, BACKGROUND_BASE);
    zm_bpm_x bpm = zm_tempo_to_bpm(editor->pattern->tempo);
    ysw_event_fire_play(editor->bus, notes, bpm);
}

static void on_demo(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    zm_composition_t *composition = ysw_array_get(editor->music->compositions, 0);
    ysw_array_t *notes = zm_render_composition(editor->music, composition, BACKGROUND_BASE);
    ysw_event_fire_play(editor->bus, notes, composition->bpm);
}

static void on_stop(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    ysw_event_fire_stop(editor->bus);
}

static void on_loop(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    editor->loop = !editor->loop;
    ysw_event_fire_loop(editor->bus, editor->loop);
}

static void on_left(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ESP_LOGD(TAG, "on_left");
    ysw_editor_t *editor = menu->context;
    process_left(editor, 1);
}

static void on_right(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    process_right(editor, 1);
}

static void on_previous(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    process_left(editor, 2);
}

static void on_next(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    process_right(editor, 2);
}

static void on_up(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    process_up(editor);
}

static void on_down(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    process_down(editor);
}

static void fire_note_on(ysw_editor_t *editor, zm_note_t midi_note, zm_channel_x channel)
{
    ysw_event_note_on_t note_on = {
        .channel = channel,
        .midi_note = midi_note,
        .velocity = 80,
    };
    ysw_event_fire_note_on(editor->bus, YSW_ORIGIN_EDITOR, &note_on);
}

static void fire_note_off(ysw_editor_t *editor, zm_note_t midi_note, zm_channel_x channel)
{
    ysw_event_note_off_t note_off = {
        .channel = channel,
        .midi_note = midi_note,
    };
    ysw_event_fire_note_off(editor->bus, YSW_ORIGIN_EDITOR, &note_off);
}

static void on_note(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    zm_note_t midi_note = (uintptr_t)value;
    if (event->header.type == YSW_EVENT_KEY_DOWN) {
        if (editor->mode == YSW_EDITOR_MODE_MELODY) {
            editor->down_at = event->key_down.time;
            editor->delta = editor->down_at - editor->up_at;
            if (midi_note) {
                fire_note_on(editor, midi_note, MELODY_CHANNEL);
            }
        } else if (editor->mode == YSW_EDITOR_MODE_CHORD) {
            zm_division_t division = {
                .chord.root = midi_note,
                .chord.quality = editor->quality,
                .chord.style = editor->style,
                .chord.duration = zm_get_ticks_per_measure(editor->pattern->time),
            };
            play_division(editor, &division);
        } else if (editor->mode == YSW_EDITOR_MODE_RHYTHM) {
            if (midi_note) {
                fire_note_on(editor, midi_note, RHYTHM_CHANNEL);
            }
        }
    } else if (event->header.type == YSW_EVENT_KEY_UP) {
        if (editor->mode == YSW_EDITOR_MODE_MELODY) {
            editor->up_at = event->key_up.time;
            if (midi_note) {
                fire_note_off(editor, midi_note, MELODY_CHANNEL);
            }
        } else if (editor->mode == YSW_EDITOR_MODE_RHYTHM) {
            if (midi_note) {
                fire_note_off(editor, midi_note, RHYTHM_CHANNEL);
            }
        }
        apply_note(editor, midi_note, event->key_pressed.duration);
    }
}

int compare_divisions(const void *left, const void *right)
{
    const zm_division_t *left_division = *(zm_division_t * const *)left;
    const zm_division_t *right_division = *(zm_division_t * const *)right;
    int delta = left_division->start - right_division->start;
    return delta;
}

static void on_note_status(ysw_editor_t *editor, ysw_event_t *event)
{
    if (event->note_status.note.channel >= BACKGROUND_BASE) {
        zm_division_t needle = {
            .start = event->note_status.note.start,
        };
        ysw_array_match_t flags = YSW_ARRAY_MATCH_EXACT;
        int32_t result = ysw_array_search(editor->pattern->divisions, &needle, compare_divisions, flags);
        if (result != -1) {
            editor->position = division_index_to_position(result);
            ysw_staff_set_position(editor->staff, editor->position);
        } else {
            // e.g. a stroke in a beat
        }
    }
}

static void on_new(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    zm_pattern_t *pattern = zm_music_create_pattern(editor->music);
    ysw_array_push(editor->music->patterns, pattern);
    ysw_event_fire_pattern_edit(editor->bus, pattern);
}

static void on_save(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    zm_save_music(editor->music);
}

static void close_chooser(ysw_editor_t *editor)
{
    if (editor->chooser) {
        ysw_menu_pop_all(editor->menu);
        ysw_chooser_delete(editor->chooser);
        editor->chooser = NULL;
    }
}

static void open_pattern(ysw_editor_t *editor)
{
    zm_pattern_t *pattern = ysw_chooser_get_pattern(editor->chooser);
    close_chooser(editor);
    if (pattern) {
        ysw_event_fire_pattern_edit(editor->bus, pattern);
    }
}

static void on_chooser_up(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    ysw_chooser_on_up(editor->chooser);
}

static void on_chooser_down(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    ysw_chooser_on_down(editor->chooser);
}

static void on_chooser_open(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    open_pattern(editor);
}

static void on_chooser_back(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    if (menu->softkeys) {
        // like close, but we pop the menu via menu flags
        ysw_editor_t *editor = menu->context;
        ysw_chooser_delete(editor->chooser);
        editor->chooser = NULL;
    }
}

static void on_chooser_event(struct _lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        uint16_t row = 0;
        uint16_t column = 0;
        ysw_editor_t *editor = lv_obj_get_user_data(obj);
        if (lv_table_get_pressed_cell(obj, &row, &column)) {
            if (row > 0) {
                zm_pattern_x new_row = row - 1; // -1 for header
                if (new_row == editor->chooser->current_row) {
#if 0
                    // open pattern on double tap
                    open_pattern(editor);
                    ysw_menu_pop_all(editor->menu);
#else
                    // on double tap, open menu (otherwise user will miss other options)
                    ysw_menu_show(editor->menu);
#endif
                } else {
                    ysw_chooser_select_row(editor->chooser, row - 1);
                }
            } else {
                ysw_menu_show(editor->menu);
            }
        }
    }
}

static void on_chooser(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    editor->chooser = ysw_chooser_create(editor->music, editor->pattern);
    lv_obj_set_user_data(editor->chooser->table, editor);
    lv_obj_set_event_cb(editor->chooser->table, on_chooser_event);
    lv_obj_set_click(editor->chooser->table, true);
    lv_obj_set_drag(editor->chooser->table, true);
    lv_obj_set_drag_dir(editor->chooser->table, LV_DRAG_DIR_VER);
}

static void on_note_length(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ysw_editor_t *editor = menu->context;
    int32_t direction = (intptr_t)value;
    ESP_LOGD(TAG, "direction=%d", direction);
    zm_division_t *division = get_division(editor);
    if (division) {
        division->melody.duration = zm_get_next_dotted_duration(division->melody.duration, direction);
        play_position(editor);
        recalculate(editor);
        display_mode(editor);
    }
}

static void on_pattern_edit(ysw_editor_t *editor, ysw_event_t *event)
{
    editor->position = 0;
    editor->pattern = event->pattern_edit.pattern;
    ysw_staff_set_pattern(editor->staff, editor->pattern);
    ysw_footer_set_key(editor->footer, editor->pattern->key);
    ysw_footer_set_time(editor->footer, editor->pattern->time);
    ysw_footer_set_tempo(editor->footer, editor->pattern->tempo);
    fire_program_change(editor, editor->pattern->melody_program, MELODY_CHANNEL);
    fire_program_change(editor, editor->pattern->chord_program, CHORD_CHANNEL);
    fire_program_change(editor, editor->pattern->rhythm_program, RHYTHM_CHANNEL);
    display_mode(editor); // mode displays program
}

static void process_event_with_pattern(ysw_editor_t *editor, ysw_event_t *event)
{
    switch (event->header.type) {
        case YSW_EVENT_KEY_DOWN:
            ysw_menu_on_key_down(editor->menu, event);
            break;
        case YSW_EVENT_KEY_UP:
            ysw_menu_on_key_up(editor->menu, event);
            break;
        case YSW_EVENT_KEY_PRESSED:
            ysw_menu_on_key_pressed(editor->menu, event);
            break;
        case YSW_EVENT_NOTE_STATUS:
            on_note_status(editor, event);
            break;
        case YSW_EVENT_PATTERN_EDIT:
            on_pattern_edit(editor, event);
            break;
        default:
            break;
    }
}

static void process_event_without_pattern(ysw_editor_t *editor, ysw_event_t *event)
{
    switch (event->header.type) {
        case YSW_EVENT_PATTERN_EDIT:
            on_pattern_edit(editor, event);
            break;
        default:
            break;
    }
}

static void process_event(void *context, ysw_event_t *event)
{
    ysw_editor_t *editor = context;
    if (event) {
        if (editor->pattern) {
            process_event_with_pattern(editor, event);
        } else {
            process_event_without_pattern(editor, event);
        }
    }
    lv_task_handler();
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

// Actions that pop a menu (e.g. COMMAND_POP) must be performed on UP, not DOWN or PRESS.

#define VP (void *)(uintptr_t)

#define YSW_MF_BUTTON (YSW_MENU_DOWN|YSW_MENU_UP)
#define YSW_MF_COMMAND (YSW_MENU_PRESS|YSW_MENU_CLOSE)
#define YSW_MF_COMMAND_POP (YSW_MENU_UP|YSW_MENU_POP_ALL)
#define YSW_MF_BUTTON_COMMAND (YSW_MENU_DOWN|YSW_MENU_UP|YSW_MENU_CLOSE)
#define YSW_MF_PLUS (YSW_MENU_UP|YSW_MENU_OPEN)
#define YSW_MF_MODE (YSW_MENU_UP|YSW_MENU_OPEN|YSW_MENU_CLOSE)
#define YSW_MF_MINUS (YSW_MENU_UP|YSW_MENU_POP)
#define YSW_MF_HIDE (YSW_MENU_CLOSE)
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

    { 36, "Hide\nMenu", YSW_MF_HIDE, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, "Menu 2" },
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

    { 36, "Hide\nMenu", YSW_MF_HIDE, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, "Melody" },
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

    { 36, "Hide\nMenu", YSW_MF_HIDE, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, "Chord" },
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

    { 36, "Hide\nMenu", YSW_MF_HIDE, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, "Rhythm" },
};

static const ysw_menu_item_t chooser_menu[] = {
    { 5, "Sort by\nName", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 6, "Sort by\nSize", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 7, "Sort by\nAge", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 8, "Up", YSW_MF_COMMAND, on_chooser_up, 0 },

    { 16, "Open\nSection", YSW_MF_COMMAND_POP, on_chooser_open, 0 },
    { 17, "Rename\nSection", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 18, "Delete\nSection", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 19, "Down", YSW_MF_COMMAND, on_chooser_down, 0 },

    { 25, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 26, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 27, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 28, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 36, "Hide\nMenu", YSW_MF_HIDE, ysw_menu_nop, 0 },
    { 37, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 38, "Menu-", YSW_MF_MINUS, on_chooser_back, 0 },
    { 39, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, "Chooser" },
};

static const ysw_menu_item_t file_menu[] = {
    { 5, "New", YSW_MF_COMMAND, on_new, 0 },
    { 6, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 7, "Save", YSW_MF_COMMAND, on_save, 0 },
    { 8, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 16, "Chooser", YSW_MF_MODE, on_chooser, (void*)chooser_menu },
    { 17, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 18, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 19, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 25, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 26, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 27, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 28, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 36, "Hide\nMenu", YSW_MF_HIDE, ysw_menu_nop, 0 },
    { 37, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 39, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, "File" },
};

static const ysw_menu_item_t edit_menu[] = {
    { 5, "Select\nLeft", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 6, "Select\nRight", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 7, "Select\nAll", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 8, "Trans\npose+", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 16, "Cut", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 17, "Copy", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 18, "Paste", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 19, "Trans\npose-", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 25, "First", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 26, "Next", YSW_MF_COMMAND, on_next, 0 },
    { 27, "Previous", YSW_MF_COMMAND, on_previous, 0 },

    { 36, "Hide\nMenu", YSW_MF_HIDE, ysw_menu_nop, 0 },
    { 37, "Delete", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, "Edit" },
};

static const ysw_menu_item_t advanced_menu[] = {
    { 5, "Edit\nSounds", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 6, "Edit\nQualities", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 7, "Edit\nStyles", YSW_MF_COMMAND, ysw_menu_nop, 0 },

    { 16, "Edit\nBeats", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 17, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 18, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 25, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 26, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 27, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 36, "Hide\nMenu", YSW_MF_HIDE, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, "Geeks Only" },
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

    { 36, "Hide\nMenu", YSW_MF_HIDE, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, "Listen" },
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

    { 36, "Hide\nMenu", YSW_MF_HIDE, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, "Settings" },
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

    { 36, "Hide\nMenu", YSW_MF_HIDE, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, "My Prefs" },
};

static const ysw_menu_item_t main_menu[] = {
    { 0, "C#6", YSW_MF_BUTTON, on_note, VP 73 },
    { 1, "D#6", YSW_MF_BUTTON, on_note, VP 75 },
    { 2, "F#6", YSW_MF_BUTTON, on_note, VP 78 },
    { 3, "G#6", YSW_MF_BUTTON, on_note, VP 80 },
    { 4, "A#6", YSW_MF_BUTTON, on_note, VP 82 },

    { 5, "Melody", YSW_MF_MODE, on_melody, (void*)melody_menu },
    { 6, "Chord", YSW_MF_MODE, on_chord, (void*)chord_menu },
    { 7, "Rhythm", YSW_MF_MODE, on_rhythm, (void*)rhythm_menu },
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
    { 18, "Geeks\nOnly", YSW_MF_PLUS, ysw_menu_nop, (void*)advanced_menu },
    { 19, "Down", YSW_MF_COMMAND, on_down, 0 },

    { 20, "C#5", YSW_MF_BUTTON, on_note, VP 61 },
    { 21, "D#5", YSW_MF_BUTTON, on_note, VP 63 },
    { 22, "F#5", YSW_MF_BUTTON, on_note, VP 66 },
    { 23, "G#5", YSW_MF_BUTTON, on_note, VP 68 },
    { 24, "A#5", YSW_MF_BUTTON, on_note, VP 70 },

    { 25, "Listen", YSW_MF_PLUS, ysw_menu_nop, (void*)listen_menu },
    { 26, "Music\nSettings", YSW_MF_PLUS, ysw_menu_nop, (void*)settings_menu },
    { 27, "My\nPrefs", YSW_MF_PLUS, ysw_menu_nop, (void*)preferences_menu },
    { 28, "Left", YSW_MF_COMMAND, on_left, 0 },

    { 29, "C5", YSW_MF_BUTTON, on_note, VP 60 },
    { 30, "D5", YSW_MF_BUTTON, on_note, VP 62 },
    { 31, "E5", YSW_MF_BUTTON, on_note, VP 64 },
    { 32, "F5", YSW_MF_BUTTON, on_note, VP 65 },
    { 33, "G5", YSW_MF_BUTTON, on_note, VP 67 },
    { 34, "A5", YSW_MF_BUTTON, on_note, VP 69 },
    { 35, "B5", YSW_MF_BUTTON, on_note, VP 71 },

    { 36, "More+", YSW_MF_PLUS, ysw_menu_nop, (void*)menu_2 },
    { 37, "Delete", YSW_MF_COMMAND, on_delete, 0 },
    { 38, "Menu-", YSW_MF_MINUS, ysw_menu_nop, 0 },
    { 39, "Right", YSW_MF_COMMAND, on_right, 0 },

    { 0, NULL, 0, NULL, "Main" },
};

static void on_glass_event(struct _lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        ysw_menu_t *menu = lv_obj_get_user_data(obj);
        ysw_menu_show(menu);
    }
}

static void initialize_editor_task(void *context)
{
    ysw_editor_t *editor = context;

    editor->lvgl_init();
    editor->container = lv_obj_create(lv_scr_act(), NULL);
    assert(editor->container);

    // See https://docs.lvgl.io/v7/en/html/overview/style.html
    lv_obj_set_style_local_bg_color(editor->container, 0, 0, LV_COLOR_MAROON);
    lv_obj_set_style_local_bg_grad_color(editor->container, 0, 0, LV_COLOR_BLACK);
    lv_obj_set_style_local_bg_grad_dir(editor->container, 0, 0, LV_GRAD_DIR_VER);
    lv_obj_set_style_local_bg_opa(editor->container, 0, 0, LV_OPA_100);

    lv_obj_set_style_local_text_color(editor->container, 0, 0, LV_COLOR_YELLOW);
    lv_obj_set_style_local_text_opa(editor->container, 0, 0, LV_OPA_100);

    lv_obj_set_size(editor->container, 320, 240);
    lv_obj_align(editor->container, NULL, LV_ALIGN_CENTER, 0, 0);

    editor->header = ysw_header_create(editor->container);
    lv_obj_set_size(editor->header, 320, 30);
    lv_obj_align(editor->header, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

    editor->staff = ysw_staff_create(editor->container);
    assert(editor->staff);
    lv_obj_set_size(editor->staff, 320, 180);
    lv_obj_align(editor->staff, NULL, LV_ALIGN_CENTER, 0, 0);

    editor->footer = ysw_footer_create(editor->container);
    lv_obj_set_size(editor->footer, 320, 30);
    lv_obj_align(editor->footer, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    ysw_footer_set_duration(editor->footer, editor->duration);

    editor->menu = ysw_menu_create(main_menu, softmap, editor);

    // We don't use lv_layer_top to detect click because it would block menu clicks
    lv_obj_t *glass = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(glass, 320, 240);
    lv_obj_align(glass, NULL, LV_ALIGN_CENTER, 0, 0);

    lv_obj_set_user_data(glass, editor->menu);
    lv_obj_set_event_cb(glass, on_glass_event);
    lv_obj_set_click(glass, true);
}

void ysw_editor_create_task(ysw_bus_t *bus, zm_music_t *music, ysw_editor_lvgl_init lvgl_init)
{
    assert(bus);
    assert(music);
    assert(lvgl_init);

    assert(ysw_array_get_count(music->programs));
    assert(ysw_array_get_count(music->qualities) > DEFAULT_QUALITY);
    assert(ysw_array_get_count(music->styles));

    ysw_editor_t *editor = ysw_heap_allocate(sizeof(ysw_editor_t));

    editor->bus = bus;
    editor->lvgl_init = lvgl_init;
    editor->music = music;

    editor->quality = ysw_array_get(music->qualities, DEFAULT_QUALITY);
    editor->style = ysw_array_get(music->styles, DEFAULT_STYLE);
    editor->beat = ysw_array_get(music->beats, DEFAULT_BEAT);
    editor->duration = ZM_AS_PLAYED;

    editor->advance = 2;
    editor->position = 0;
    editor->mode = YSW_EDITOR_MODE_MELODY;

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.initializer = initialize_editor_task;
    config.event_handler = process_event;
    config.context = editor;
    config.wait_millis = 5;
    //config.priority = YSW_TASK_DEFAULT_PRIORITY + 1;
    //config.priority = configMAX_PRIORITIES - 1;
    config.priority = 24;

    ysw_task_t *task = ysw_task_create(&config);

    ysw_task_subscribe(task, YSW_ORIGIN_FILE);
    ysw_task_subscribe(task, YSW_ORIGIN_KEYBOARD);
    ysw_task_subscribe(task, YSW_ORIGIN_SEQUENCER);
}

