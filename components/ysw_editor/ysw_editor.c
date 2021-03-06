// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_editor.h"
#include "ysw_app.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_footer.h"
#include "ysw_header.h"
#include "ysw_heap.h"
#include "ysw_menu.h"
#include "ysw_midi.h"
#include "ysw_popup.h"
#include "ysw_staff.h"
#include "ysw_string.h"
#include "ysw_style.h"
#include "ysw_task.h"
#include "ysw_ticks.h"
#include "zm_music.h"
#include "lvgl.h"
#include "esp_log.h"
#include "assert.h"
#include "limits.h"
#include "stdlib.h"

#define TAG "YSW_EDITOR"

#define DEFAULT_CHORD_TYPE 0
#define DEFAULT_CHORD_STYLE 0
#define DEFAULT_BEAT 0

// channels

#define BASE_CHANNEL 0
#define MELODY_CHANNEL (BASE_CHANNEL+0)
#define CHORD_CHANNEL (BASE_CHANNEL+1)
#define RHYTHM_CHANNEL (BASE_CHANNEL+2)

#define BACKGROUND_BASE (BASE_CHANNEL+3)
#define BACKGROUND_MELODY (BACKGROUND_BASE+0)
#define BACKGROUND_CHORD (BACKGROUND_BASE+1)
#define BACKGROUND_RHYTHM (BACKGROUND_BASE+2)

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

typedef struct {
    zm_octave_t octave;
    zm_note_t semis;
} insert_settings;

#define YSW_EDITOR_MODE_COUNT 3

// TODO: lookup chord type by name

#define MAJOR 0
#define MINOR 1
#define DOMINANT 4
#define AUGMENTED 5
#define DIMINISHED 6

typedef struct {
    ysw_bus_t *bus;
    ysw_menu_t *menu;
    QueueHandle_t queue;

    zm_music_t *music;
    zm_section_t *section;
    zm_chord_type_t *chord_type;
    zm_chord_style_t *chord_style;
    zm_chord_frequency_t chord_frequency;
    zm_beat_t *beat;
    zm_duration_t duration;
    zm_duration_t drum_cadence;

    zm_section_t *original_section;

    zm_time_x down_at;
    zm_time_x up_at;
    zm_time_x delta;

    bool loop;
    bool insert;
    bool modified;
    bool harp;
    zm_step_x position;
    ysw_editor_mode_t mode;

    lv_obj_t *container;
    lv_obj_t *header;
    lv_obj_t *staff;
    lv_obj_t *footer;

    ysw_popup_t *popup;

    insert_settings insert_settings;
    zm_chord_t chord_builder;
    ysw_editor_mode_t program_builder;

} ysw_editor_t;

static ysw_array_t *clipboard; // shared clipboard

// Note that position is 2x the step index and encodes both the step index and the space before it.

// 0 % 2 = 0
// 1 % 2 = 1 C
// 2 % 2 = 0
// 3 % 2 = 1 E
// 4 % 2 = 0
// 5 % 2 = 1 G
// 6 % 2 = 0

// A remainder of zero indicates the space and a remainder of 1 indicates
// the step. This happens to work at the end of the array too, if the position
// is 2x the size of the array, it refers to the space following the last step.

// if (position / 2 >= size) {
//   -> space after last step
// } else {
//   if (position % 2 == 0) {
//     -> space before step
//   } else {
//     -> step_index = position / 2;
//   }
// }

static inline bool is_step(uint32_t index)
{
    return index % 2 == 1;
}

static inline bool is_step_position(ysw_editor_t *editor)
{
    return is_step(editor->position);
}

static inline bool is_space(uint32_t index)
{
    return index % 2 == 0;
}
 
static inline bool is_space_position(ysw_editor_t *editor)
{
    return is_space(editor->position);
}
 
static inline uint32_t step_index_to_position(zm_step_x step_index)
{
    return (step_index * 2) + 1;
}

static zm_step_t *get_step(ysw_editor_t *editor)
{
    zm_step_t *step = NULL;
    if (is_step_position(editor)) {
        zm_step_x step_index = editor->position / 2;
        step = ysw_array_get(editor->section->steps, step_index);
    }
    return step;
}

static zm_step_t *get_closest_step(ysw_editor_t *editor)
{
    zm_step_t *step = NULL;
    zm_step_x step_count = ysw_array_get_count(editor->section->steps);
    if (step_count) {
        zm_step_x step_index = editor->position / 2;
        if (step_index >= step_count) {
            step_index = step_count - 1;
        }
        step = ysw_array_get(editor->section->steps, step_index);
    }
    return step;
}

static void save_undo_action(ysw_editor_t *editor)
{
    editor->modified = true;
    // TODO: add undo/redo support
}

static void recalculate(ysw_editor_t *editor)
{
    zm_recalculate_section(editor->section);
    ysw_staff_invalidate(editor->staff);
}

static void play_step(ysw_editor_t *editor, zm_step_t *step)
{
    ysw_array_t *notes = zm_render_step(editor->music, editor->section, step, BASE_CHANNEL);
    ysw_event_fire_play(editor->bus, notes, editor->section->tempo);
}

static void play_position(ysw_editor_t *editor)
{
    zm_step_t *step = get_step(editor);
    if (step) {
        play_step(editor, step);
    }
}

static void play_note(ysw_editor_t *editor, zm_note_t midi_note, zm_time_x ticks)
{
    zm_step_t step = {
        .melody.note = midi_note,
        .melody.duration = ticks
    };
    play_step(editor, &step);
}

static void play_chord(ysw_editor_t *editor, zm_chord_t *chord)
{
    zm_step_t step = {
        .chord.root = chord->root,
        .chord.type = chord->type,
        .chord.style = chord->style,
        .chord.duration = zm_get_ticks_per_measure(editor->section->time) / chord->frequency,
    };
    play_step(editor, &step);
}

static void play_beat(ysw_editor_t *editor, zm_beat_t *beat)
{
    zm_step_t step = {
        .rhythm.beat = beat,
    };
    play_step(editor, &step);
}

static void play_stroke(ysw_editor_t *editor, zm_note_t surface)
{
    zm_step_t step = {
        .rhythm.surface = surface,
        .rhythm.cadence = editor->drum_cadence,
    };
    play_step(editor, &step);
}

static void display_program(ysw_editor_t *editor)
{
    const char *value = "";
    if (editor->mode == YSW_EDITOR_MODE_MELODY) {
        value = ysw_midi_get_program_name(editor->section->melody_program);
    } else if (editor->mode == YSW_EDITOR_MODE_CHORD) {
        value = ysw_midi_get_program_name(editor->section->chord_program);
    } else if (editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        value = "Drums";
    }
    ysw_header_set_program(editor->header, value);
}

static void display_melody_mode(ysw_editor_t *editor)
{
    // 1. on a step - show note or rest
    // 2. between steps - show previous (to left) note or rest
    // 3. no steps - show blank
    char value[32];
    if (editor->position > 0) {
        zm_step_t *step = get_closest_step(editor);
        uint32_t millis = ysw_ticks_to_millis(step->melody.duration, editor->section->tempo);
        zm_note_t note = step->melody.note;
        if (note) {
            zm_octave_t octave = (note / 12) - 1;
            const char *name = zm_get_note_name(note);
            snprintf(value, sizeof(value), "%s%d (%d ms)", name, octave, millis);
        } else {
            snprintf(value, sizeof(value), "Rest (%d ms)", millis);
        }
    } else {
        snprintf(value, sizeof(value), "%s", editor->section->name);
    }
    if (editor->harp) {
        ysw_header_set_mode(editor->header, "Harp", value);
    } else {
        ysw_header_set_mode(editor->header, modes[editor->mode], value);
    }
}

static void display_chord_mode(ysw_editor_t *editor)
{
    // 1. on a step with a chord value - show chord value, chord_type, chord_style
    // 2. on a step without a chord value - show chord_type, chord_style
    // 3. between steps - show chord_type, chord_style
    // 4. no steps - show chord_type, chord_style
    char value[32] = {};
    zm_step_t *step = get_step(editor);
    if (step && step->chord.root) {
        snprintf(value, sizeof(value), "%s %s/%s",
                zm_get_note_name(step->chord.root),
                step->chord.type->name,
                step->chord.style->name);
    } else {
        snprintf(value, sizeof(value), "%s/%s",
                editor->chord_type->name,
                editor->chord_style->name);
    }
    ysw_header_set_mode(editor->header, modes[editor->mode], value);
}

static void display_rhythm_mode(ysw_editor_t *editor)
{
    char value[32] = {};
    zm_step_t *step = get_step(editor);
    if (step && step->rhythm.beat) {
        snprintf(value, sizeof(value), "%s", step->rhythm.beat->name);
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

static void set_position(ysw_editor_t *editor, int32_t position)
{
    if (position < 0) {
        position = 0;
    } else if (position) {
        uint32_t rightmost_position = 0;
        zm_step_x step_count = ysw_array_get_count(editor->section->steps);
        if (step_count) {
            rightmost_position = step_index_to_position(step_count - 1); // -1 for index
            rightmost_position++; // space following step
        }
        if (position > rightmost_position) {
            position = rightmost_position;
        }
    }
    editor->position = position;
    play_position(editor);
    ysw_staff_set_position(editor->staff, position, YSW_STAFF_SCROLL);
    display_mode(editor);
}

static void fire_bank_select(ysw_editor_t *editor, zm_channel_x channel, zm_bank_x bank)
{
    ysw_event_bank_select_t bank_select = {
        .channel = channel,
        .bank = bank,
    };
    ysw_event_fire_bank_select(editor->bus, YSW_ORIGIN_EDITOR, &bank_select);
}

static void fire_program_change(ysw_editor_t *editor, zm_channel_x channel, zm_program_x program)
{
    ysw_event_program_change_t program_change = {
        .channel = channel,
        .program = program,
        .preload = true,
    };
    ysw_event_fire_program_change(editor->bus, YSW_ORIGIN_EDITOR, &program_change);
}

static void fire_note_on(ysw_editor_t *editor, zm_channel_x channel, zm_note_t midi_note)
{
    ysw_event_note_on_t note_on = {
        .channel = channel,
        .midi_note = midi_note,
        .velocity = 80,
    };
    ysw_event_fire_note_on(editor->bus, YSW_ORIGIN_EDITOR, &note_on);
}

static void fire_note_off(ysw_editor_t *editor, zm_channel_x channel, zm_note_t midi_note)
{
    ysw_event_note_off_t note_off = {
        .channel = channel,
        .midi_note = midi_note,
    };
    ysw_event_fire_note_off(editor->bus, YSW_ORIGIN_EDITOR, &note_off);
}

// Setters

static void set_program(ysw_editor_t *editor, ysw_editor_mode_t mode, zm_program_x program)
{
    switch (mode) {
        case YSW_EDITOR_MODE_MELODY:
            editor->section->melody_program = program;
            fire_program_change(editor, MELODY_CHANNEL, editor->section->melody_program);
            break;
        case YSW_EDITOR_MODE_CHORD:
            editor->section->chord_program = program;
            fire_program_change(editor, CHORD_CHANNEL, editor->section->chord_program);
            break;
        case YSW_EDITOR_MODE_RHYTHM:
            // no program change for rhythm channel
            break;
    }
    display_program(editor);
    //play_position(editor);
}

static void set_key_signature(ysw_editor_t *editor, zm_key_signature_x key)
{
    editor->section->key = key;
    ysw_footer_set_key(editor->footer, editor->section->key);
    ysw_staff_invalidate(editor->staff);
    save_undo_action(editor);
}

static void set_tie(ysw_editor_t *editor, zm_tie_x tie)
{
    if (is_step_position(editor)) {
        zm_step_x step_index = editor->position / 2;
        zm_step_t *step = ysw_array_get(editor->section->steps, step_index);
        if (step->melody.note) {
            step->melody.tie = tie;
            ysw_staff_invalidate(editor->staff);
            save_undo_action(editor);
        }
    }
}

static void set_time_signature(ysw_editor_t *editor, zm_time_signature_x time)
{
    editor->section->time = time;
    ysw_footer_set_time(editor->footer, editor->section->time);
    recalculate(editor);
    save_undo_action(editor);
}

static void set_tempo(ysw_editor_t *editor, zm_tempo_x tempo)
{
    editor->section->tempo = tempo;
    ysw_footer_set_tempo(editor->footer, editor->section->tempo);
    display_mode(editor);
    save_undo_action(editor);
}

static void set_default_note_duration(ysw_editor_t *editor, zm_duration_t duration)
{
    zm_step_t *step = NULL;
    if (editor->duration != ZM_AS_PLAYED) {
        step = get_step(editor); // null if on a space
    }
    // Set default duration if not on a step or if step duration is already default duration
    if (!step || step->melody.duration == editor->duration) {
        editor->duration = duration;
        ysw_footer_set_duration(editor->footer, editor->duration);
    }
    if (step) {
        step->melody.duration = editor->duration;
        display_mode(editor);
        recalculate(editor);
        save_undo_action(editor);
    }
}

static zm_chord_style_t *find_matching_style(ysw_editor_t *editor, bool preincrement)
{
    zm_chord_style_x current = ysw_array_find(editor->music->chord_styles, editor->chord_style);
    zm_chord_style_x style_count = ysw_array_get_count(editor->music->chord_styles);
    zm_distance_x distance_count = ysw_array_get_count(editor->chord_type->distances);
    for (zm_chord_style_x i = 0; i < style_count; i++) {
        if (preincrement) {
            current = (current + 1) % style_count;
        }
        zm_chord_style_t *style = ysw_array_get(editor->music->chord_styles, current);
        if (style->distance_count == distance_count) {
            editor->chord_style = style;
            return style;
        }
        if (!preincrement) {
            current = (current + 1) % style_count;
        }
    }
    ESP_LOGE(TAG, "no matching style for chord with distance_count=%d", distance_count);
    abort();
}

static void set_chord_type(ysw_editor_t *editor, zm_chord_type_x chord_type_x)
{
    zm_step_t *step = get_step(editor); // null if on a space
    if (!step || !step->chord.root || step->chord.type == editor->chord_type) {
        editor->chord_type = ysw_array_get(editor->music->chord_types, chord_type_x);
        editor->chord_style = find_matching_style(editor, false);
    }
    if (step) {
        step->chord.type = editor->chord_type;
        step->chord.style = editor->chord_style;
        ysw_staff_invalidate(editor->staff);
        save_undo_action(editor);
    }
    display_mode(editor);
    play_position(editor);
}

static void set_chord_style(ysw_editor_t *editor, zm_chord_style_x chord_style_x)
{
    zm_step_t *step = get_step(editor); // null if on a space
    if (!step || !step->chord.root || step->chord.style == editor->chord_style) {
        editor->chord_style = ysw_array_get(editor->music->chord_styles, chord_style_x);
    }
    if (step) {
        step->chord.style = editor->chord_style;
    }
    display_mode(editor);
    play_position(editor);
}

// Steps

static inline bool are_within(uint32_t left, uint32_t right, uint32_t range)
{
    return left > right ? (left - right) < range : (right - left) < range;
}

static inline zm_time_x ysw_abs_time(zm_time_x left, zm_time_x right)
{
    return left > right ? left - right : right - left;
}

static zm_step_t *find_closest_step(ysw_array_t *steps, zm_step_x step_index, zm_time_x start)
{
    bool done = false;
    zm_time_x shortest_delta = INT_MAX;
    zm_step_t *shortest_step = NULL;
    zm_step_x step_count = ysw_array_get_count(steps);
    for (zm_step_x i = step_index; i < step_count && !done; i++) {
        zm_step_t *step = ysw_array_get(steps, i);
        zm_time_x this_delta = ysw_abs_time(step->start, start);
        if (this_delta < shortest_delta) {
            shortest_delta = this_delta;
            shortest_step = step;
        } else {
            done = true;
        }
    }
    return shortest_step;
}

static zm_step_t *realize_step(ysw_editor_t *editor, zm_step_x *step_index_p, zm_time_x start)
{
    zm_step_t *step = NULL;
    int32_t step_index = editor->position / 2;
    bool is_new_step = is_space_position(editor);
    if (is_new_step) {
        step = ysw_heap_allocate(sizeof(zm_step_t));
        ysw_array_insert(editor->section->steps, step_index, step);
    } else {
        if (start) {
            step = find_closest_step(editor->section->steps, step_index, start);
        } else {
            step = ysw_array_get(editor->section->steps, step_index);
        }
    }
    if (step_index_p) {
        *step_index_p = step_index;
    }
    return step;
}

static void finalize_step(ysw_editor_t *editor, zm_step_x step_index)
{
    zm_step_x step_count = ysw_array_get_count(editor->section->steps);
#if 0
    // insert always
    uint32_t position = min((step_index * 2) + 2, step_count * 2);
#else
    // insert when starting on space, overtype when starting on step
    uint32_t position = min(editor->position + 2, step_count * 2);
#endif
    set_position(editor, position);
    recalculate(editor);
    save_undo_action(editor);
}

static zm_step_t *find_previous_melody_step(ysw_editor_t *editor, int32_t step_index)
{
    while (--step_index > 0) {
        zm_step_t *previous_step = ysw_array_get(editor->section->steps, step_index);
        if (previous_step->melody.note) {
            return previous_step;
        }
    }
    return NULL;
}

static void realize_note(ysw_editor_t *editor, zm_note_t midi_note, zm_time_x duration_millis)
{
    zm_time_x start = 0;
    zm_step_x step_index = editor->position / 2;
    zm_duration_t duration = editor->duration;

    if (duration == ZM_AS_PLAYED) {
        duration = ysw_millis_to_ticks(duration_millis, editor->section->tempo);
    }

    zm_step_t *previous_step = find_previous_melody_step(editor, step_index);
    if (previous_step) {
        start = previous_step->start + previous_step->melody.duration;
        zm_time_x articulation = ysw_millis_to_ticks(editor->delta, editor->section->tempo);
        if (articulation < ZM_WHOLE && editor->duration == ZM_AS_PLAYED) {
            if (is_space_position(editor)) {
                previous_step->melody.duration += articulation;
            }
        } else {
            articulation = 0;
        }
        start += articulation;
    }

    zm_step_t *step = realize_step(editor, &step_index, start);

    step->melody.note = midi_note; // rest == 0
    step->melody.duration = duration;

    finalize_step(editor, step_index);
}

static void realize_chord(ysw_editor_t *editor, zm_chord_t *chord)
{
    zm_step_x step_index;
    zm_step_t *step = realize_step(editor, &step_index, 0);

    step->chord.root = chord->root;
    step->chord.type = chord->type;
    step->chord.style = chord->style;
    step->chord.frequency = chord->frequency;
    step->chord.duration = 0; // set by recalculate

    finalize_step(editor, step_index);
}

static void realize_stroke(ysw_editor_t *editor, zm_note_t surface)
{
    zm_step_x step_index;
    zm_step_t *step = realize_step(editor, &step_index, 0);

    step->rhythm.surface = surface;
    step->rhythm.cadence = editor->drum_cadence;

    finalize_step(editor, step_index);
}

static void realize_beat(ysw_editor_t *editor, zm_beat_t *beat)
{
    zm_step_x step_index;
    zm_step_t *step = realize_step(editor, &step_index, 0);

    step->rhythm.beat = beat;

    finalize_step(editor, step_index);
}

static void realize_rest(ysw_editor_t *editor, zm_time_x duration_ticks)
{
    zm_step_x step_index;
    zm_step_t *step = realize_step(editor, &step_index, 0);

    step->melody.note = 0;
    step->melody.duration = duration_ticks;

    finalize_step(editor, step_index);
}

static void press_note(ysw_editor_t *editor, zm_note_t midi_note, uint32_t down_at)
{
    if (editor->mode == YSW_EDITOR_MODE_MELODY) {
        editor->down_at = down_at;
        editor->delta = editor->down_at - editor->up_at;
        if (midi_note) {
            fire_note_on(editor, MELODY_CHANNEL, midi_note);
        }
    } else if (editor->mode == YSW_EDITOR_MODE_CHORD) {
        zm_step_t step = {
            .chord.root = midi_note,
            .chord.type = editor->chord_type,
            .chord.style = editor->chord_style,
            .chord.duration = zm_get_ticks_per_measure(editor->section->time),
        };
        play_step(editor, &step);
    } else if (editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        if (midi_note) {
            fire_note_on(editor, RHYTHM_CHANNEL, midi_note);
        }
    }
}

static void release_note(ysw_editor_t *editor, zm_note_t midi_note, uint32_t up_at, uint32_t duration)
{
    if (editor->mode == YSW_EDITOR_MODE_MELODY) {
        editor->up_at = up_at;
        if (midi_note) {
            fire_note_off(editor, MELODY_CHANNEL, midi_note);
        }
        realize_note(editor, midi_note, duration);
    } else if (editor->mode == YSW_EDITOR_MODE_CHORD) {
        zm_chord_t chord = {
            .root = midi_note,
            .type = editor->chord_type,
            .style = editor->chord_style,
            .frequency = editor->chord_frequency,
        };
        realize_chord(editor, &chord);
    } else if (editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        if (midi_note) {
            fire_note_off(editor, RHYTHM_CHANNEL, midi_note);
        }
        realize_stroke(editor, midi_note);
    }
}

static void delete_step(ysw_editor_t *editor)
{
    zm_step_x step_count = ysw_array_get_count(editor->section->steps);
    if (step_count) {
        zm_step_x step_index = editor->position / 2;
        if (step_index == step_count) {
            step_index = step_count - 1; // on rightmost space, so delete previous
        }
        zm_step_t *step = ysw_array_remove(editor->section->steps, step_index);
        ysw_heap_free(step);
        set_position(editor, editor->position); // will adjust if necessary
        recalculate(editor);
        save_undo_action(editor);
    }
}

static void transpose(ysw_editor_t *editor, zm_range_t *range, int32_t delta)
{
    if (zm_transpose_section(editor->section, range, delta)) {
        play_position(editor);
        display_mode(editor);
        ysw_staff_invalidate(editor->staff);
        save_undo_action(editor);
    }
}

static bool get_range(ysw_editor_t *editor, zm_range_t *range)
{
    zm_step_x step_count = ysw_array_get_count(editor->section->steps);
    if (!step_count) {
        range->first = 0;
        range->last = -1;
        return false;
    }

    int32_t first = 0;
    int32_t last = 0;
    int32_t anchor = ysw_staff_get_anchor(editor->staff);
    if (anchor == YSW_STAFF_NO_ANCHOR) {
        first = editor->position;
        last = editor->position;
    } else if (anchor < editor->position) {
        first = anchor;
        last = editor->position;
    } else {
        first = editor->position;
        last = anchor;
    }

    range->first = min(first / 2, step_count - 1);
    range->last = min(last / 2, step_count - 1);
    return true;
}

// Generators

#define YSW_APP_SOFTKEY_SZ 16

// TODO: use ysw_app_softkey_map_t

static const uint8_t softkey_map[] = {
    YSW_R1_C1,
    YSW_R1_C2,
    YSW_R1_C3,
    YSW_R1_C4,

    YSW_R2_C1,
    YSW_R2_C2,
    YSW_R2_C3,
    YSW_R2_C4,

    YSW_R3_C1,
    YSW_R3_C2,
    YSW_R3_C3,
    YSW_R3_C4,

    YSW_R4_C1,
    YSW_R4_C2,
    YSW_R4_C3,
    YSW_R4_C4,
};

static void initialize_item(ysw_menu_item_t *item, uint32_t softkey, const char *name, ysw_menu_cb_t cb, intptr_t value, const ysw_menu_item_t *submenu)
{
    item->softkey = softkey;
    item->name = name;
    item->flags = submenu ? YSW_MF_PLUS : YSW_MF_COMMAND;
    item->cb = cb;
    item->value = value;
    item->submenu = submenu;
}

static void finalize_menu(ysw_menu_item_t *p, const char *name)
{
    p->softkey = YSW_R4_C1;
    p->name = "Back";
    p->flags = YSW_MF_MINUS;
    p->cb = ysw_menu_nop;
    p->value = 0;
    p->submenu = NULL;
    p++;

    p->name = name;
    p->flags = YSW_MF_END;
}

static ysw_menu_item_t chord_style_template[YSW_APP_SOFTKEY_SZ + 1];

static const ysw_menu_item_t chord_duration_menu[]; // forward declaration

static void generate_chord_style_menu(ysw_editor_t *editor, ysw_menu_cb_t cb, ysw_menu_item_t *template, const ysw_menu_item_t *submenu, const char *name)
{
    zm_distance_x distance_count = ysw_array_get_count(editor->chord_builder.type->distances);
    zm_chord_style_x style_count = ysw_array_get_count(editor->music->chord_styles);
    // TODO: create multiple chord style menus if necessary
    ysw_menu_item_t *p = template;
    for (zm_chord_style_x i = 0; i < style_count && (p - template) < 12; i++) {
        zm_chord_style_t *style = ysw_array_get(editor->music->chord_styles, i);
        if (style->distance_count == distance_count) {
            initialize_item(p, softkey_map[p - template], style->label, cb, i, submenu);
            p++;
        }
    }
    finalize_menu(p, name);
}

static ysw_menu_item_t chord_type_template[YSW_APP_SOFTKEY_SZ + 1];

static void generate_chord_type_menu(ysw_editor_t *editor, ysw_menu_cb_t cb, ysw_menu_item_t *template, const ysw_menu_item_t *submenu, const char *name)
{
    zm_chord_type_x count = ysw_array_get_count(editor->music->chord_types);
    // TODO: use multiple chord type menus if necessary
    count = min(count, 12);
    ysw_menu_item_t *p = template;
    for (zm_chord_type_x i = 0; i < count; i++) {
        zm_chord_type_t *chord_type = ysw_array_get(editor->music->chord_types, i);
        initialize_item(p, softkey_map[i], chord_type->label, cb, i, submenu);
        p++;
    }
    finalize_menu(p, name);
}

static void generate_program_menu(ysw_editor_t *editor, ysw_menu_cb_t cb, ysw_menu_item_t *template, uint8_t category, const char *name)
{
    ysw_menu_item_t *p = template;
    for (zm_chord_style_x i = 0; i < 8; i++) {
        uint8_t program = (category * 8) + i;
        const char *label = ysw_midi_get_program_label(program);
        initialize_item(p, softkey_map[p - template], label, cb, program, NULL);
        p++;
    }
    finalize_menu(p, name);
}

static void generate_beat_menu(ysw_editor_t *editor, ysw_menu_cb_t cb, ysw_menu_item_t *template, const ysw_menu_item_t *submenu, const char *name)
{
    zm_beat_x count = ysw_array_get_count(editor->music->beats);
    // TODO: use multiple beat menus if necessary
    ysw_menu_item_t *p = template;
    for (zm_chord_style_x i = 0; i < count && (p - template) < 12; i++) {
        zm_beat_t *beat = ysw_array_get(editor->music->beats, i);
        initialize_item(p, softkey_map[p - template], beat->label, cb, i, submenu);
        p++;
    }
    finalize_menu(p, name);
}

static void generate_surface_menu(ysw_editor_t *editor, ysw_menu_cb_t cb, ysw_menu_item_t *template, const ysw_menu_item_t *submenu, const char *name)
{
    // TODO: use midi drum names
    ysw_menu_item_t *p = template;
    for (zm_chord_style_x i = 0; i < 12 && (p - template) < 12; i++) {
        initialize_item(p, softkey_map[p - template], ysw_midi_get_drum_name(i), cb, 35 + i, submenu);
        p++;
    }
    finalize_menu(p, name);
}

static const char *flats[] = {
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
};

static const char *sharps[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

static ysw_menu_item_t diatonic_template[YSW_APP_SOFTKEY_SZ + 1];
static ysw_menu_item_t chromatic_template[YSW_APP_SOFTKEY_SZ + 1];

static void on_octave(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item);

static void generate_scale_notes_menu(ysw_editor_t *editor, ysw_menu_cb_t cb, const ysw_menu_item_t *submenu, const char *name)
{
    ysw_menu_item_t *p = diatonic_template;
    ysw_menu_item_t *q = chromatic_template;
    const zm_key_signature_t *ks = zm_get_key_signature(editor->section->key);
    const zm_scale_t *scale = &zm_scales[editor->section->key]; // NB not ks->tonic_semis!
    ESP_LOGD(TAG, "key=%s", ks->name);
    for (zm_note_t i = 0; i < 12; i++) {
        if (scale->semitone[i] > 0) { // diatonic
            int8_t degree = scale->semitone[i] - 1; // -1 because scale semitones are 1-based
            const char *name = NULL;
            if (ks->flats) {
                name = flats[degree];
            } else {
                name = sharps[degree];
            }
            initialize_item(p, softkey_map[p - diatonic_template], name, cb, degree, submenu);
            ESP_LOGD(TAG, "p name=%s, semi=%d", name, degree);
            p++;
        } else { // chromatic
            int8_t degree = -scale->semitone[i] - 1; // -1 because scale semitones are 1-based
            const char *name = NULL;
            if (ks->flats) {
                name = flats[degree];
            } else {
                name = sharps[degree];
            }
            initialize_item(q, softkey_map[q - chromatic_template], name, cb, degree, submenu);
            ESP_LOGD(TAG, "q name=%s, semi=%d", name, degree);
            q++;
        }
    }
    *p++ = (ysw_menu_item_t) { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL };
    *p++ = (ysw_menu_item_t) { YSW_R4_C2, "Octave-", YSW_MF_PRESS, on_octave, -1, NULL };
    *p++ = (ysw_menu_item_t) { YSW_R4_C3, "Octave+", YSW_MF_PRESS, on_octave, +1, NULL };
    *p++ = (ysw_menu_item_t) { YSW_R4_C4, "Chro-\nmatic", YSW_MF_PLUS, ysw_menu_nop, +1, chromatic_template };
    *p++ = (ysw_menu_item_t) { 0, name, YSW_MF_END, NULL, 0, NULL };

    *q++ = (ysw_menu_item_t) { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL };
    *q++ = (ysw_menu_item_t) { YSW_R4_C2, "Octave-", YSW_MF_PRESS, on_octave, -1, NULL };
    *q++ = (ysw_menu_item_t) { YSW_R4_C3, "Octave+", YSW_MF_PRESS, on_octave, +1, NULL };
    *q++ = (ysw_menu_item_t) { 0, "Chromatic", YSW_MF_END, NULL, 0, NULL };

}

// Handlers

static void on_mode_melody(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->mode = YSW_EDITOR_MODE_MELODY;
    display_mode(editor);
}

static void on_mode_chord(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->mode = YSW_EDITOR_MODE_CHORD;
    display_mode(editor);
}

static void on_mode_rhythm(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->mode = YSW_EDITOR_MODE_RHYTHM;
    display_mode(editor);
}

static void on_mode_harp(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->mode = YSW_EDITOR_MODE_MELODY;
    editor->harp = !editor->harp;
    display_mode(editor);
}

static ysw_menu_item_t program_template[YSW_APP_SOFTKEY_SZ + 1];

static void on_program_3(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    set_program(editor, editor->program_builder, item->value);
}

static void on_program_2(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    generate_program_menu(editor, on_program_3, program_template, item->value, "Program");
}

static void on_program_1(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->program_builder = item->value;
}

static void on_settings_chord_type_2(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    set_chord_type(editor, item->value);
}

static void on_settings_chord_type_1(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    generate_chord_type_menu(editor, on_settings_chord_type_2, chord_type_template, NULL, "Type");
}

static void on_settings_chord_style_2(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    set_chord_style(editor, item->value);
}

static void on_settings_chord_style_1(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->chord_builder.type = editor->chord_type; // used by generate_chord_style_menu
    generate_chord_style_menu(editor, on_settings_chord_style_2, chord_style_template, NULL, "Style");
}

static void on_key_signature(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    set_key_signature(editor, item->value);
}

static void on_time_signature(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    set_time_signature(editor, item->value);
}

static void on_tempo(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    set_tempo(editor, item->value);
}

static void on_default_note_duration(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    set_default_note_duration(editor, item->value);
}

static void on_default_chord_frequency(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->chord_frequency = item->value;
}

static void on_default_drum_cadence(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->drum_cadence = item->value;
}

static void on_insert_tie(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    set_tie(editor, 1);
}

static void on_rest_duration(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    realize_rest(editor, item->value);
}

static void on_delete(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    delete_step(editor);
}

static void on_play(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    ysw_array_t *notes = zm_render_section(editor->music, editor->section, BACKGROUND_BASE);
    ysw_event_fire_play(editor->bus, notes, editor->section->tempo);
}

static void on_headphone_volume(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    ysw_event_fire_amp_volume(editor->bus, item->value);
#ifdef IDF_VER
    // TODO: move this to a codec task
    ESP_LOGD(TAG, "headphone_volume=%d", item->value);
    extern esp_err_t ac101_set_earph_volume(int volume);
	$(ac101_set_earph_volume(((item->value % 101) * 63) / 100));
#endif
}

static void on_speaker_volume(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    ysw_event_fire_amp_volume(editor->bus, item->value);
#ifdef IDF_VER
    // TODO: move this to a codec task
    ESP_LOGD(TAG, "speaker_volume=%d", item->value);
    extern esp_err_t ac101_set_spk_volume(int volume);
	$(ac101_set_spk_volume(((item->value % 101) * 63) / 100));
#endif
}
static void on_synth_gain(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    ysw_event_fire_synth_gain(editor->bus, item->value);
}

static void on_demo(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    zm_composition_t *composition = ysw_array_get(editor->music->compositions, 0);
    ysw_array_t *notes = zm_render_composition(editor->music, composition, BACKGROUND_BASE);
    ysw_event_fire_play(editor->bus, notes, composition->bpm);
}

static void on_stop(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    ysw_event_fire_stop(editor->bus);
}

static void on_loop(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->loop = !editor->loop;
    ysw_event_fire_loop(editor->bus, editor->loop);
}

static void on_left(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    set_position(editor, editor->position - 1);
}

static void on_right(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    set_position(editor, editor->position + 1);
}

static void on_edit_previous(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    if (is_space_position(editor)) {
        set_position(editor, editor->position - 1);
    } else {
        set_position(editor, editor->position - 2);
    }
}

static void on_edit_next(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    if (is_space_position(editor)) {
        set_position(editor, editor->position + 1);
    } else {
        set_position(editor, editor->position + 2);
    }
}

static void on_edit_pitch(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    zm_range_t range;
    ysw_editor_t *editor = menu->context;
    if (get_range(editor, &range)) {
        transpose(editor, &range, item->value);
    }
}

static void on_edit_length(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    zm_range_t range;
    ysw_editor_t *editor = menu->context;
    if (get_range(editor, &range)) {
        for (zm_step_x i = range.first; i < range.last; i++) {
            zm_step_t *step = ysw_array_get(editor->section->steps, i);
            step->melody.duration = zm_get_next_dotted_duration(step->melody.duration, item->value);
        }
        play_position(editor);
        display_mode(editor);
        recalculate(editor);
        save_undo_action(editor);
    }
}

static void on_edit_drop_anchor(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    ysw_staff_set_anchor(editor->staff, editor->position);
}

static void on_edit_clear_anchor(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    ysw_staff_set_anchor(editor->staff, YSW_STAFF_NO_ANCHOR);
}

typedef enum {
    EDIT_CUT,
    EDIT_COPY,
} clipboard_action_t;

static void on_edit_to_clipboard(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;

    zm_range_t range;
    if (!get_range(editor, &range)) {
        return;
    }

    int32_t count = range.last - range.first + 1; // +1 because it's inclusive

    ysw_array_free_elements(clipboard);

    for (int32_t i = 0; i < count; i++) {
        if (item->value == EDIT_COPY) {
            zm_step_t *step = ysw_array_get(editor->section->steps, range.first + i);
            zm_step_t *new_step = ysw_heap_allocate(sizeof(zm_step_t));
            *new_step = *step;
            ysw_array_push(clipboard, new_step);
        } else if (item->value == EDIT_CUT) {
            zm_step_t *step = ysw_array_remove(editor->section->steps, range.first);
            ysw_array_push(clipboard, step);
        }
    }

    if (item->value == EDIT_CUT) {
        set_position(editor, ysw_staff_get_anchor(editor->staff)); // will be adjusted as necessary
        recalculate(editor);
        save_undo_action(editor);
    }

    ysw_staff_set_anchor(editor->staff, YSW_STAFF_NO_ANCHOR);
}

static void on_edit_paste(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    uint32_t new_position = editor->position;
    uint32_t clipboard_count = ysw_array_get_count(clipboard);
    for (uint32_t i = 0; i < clipboard_count; i++) {
        zm_step_t *step = ysw_array_get(clipboard, i);
        zm_step_t *new_step = ysw_heap_allocate(sizeof(zm_step_t));
        *new_step = *step;
        ysw_array_insert(editor->section->steps, new_position / 2, new_step);
        new_position += 2;
    }
    
    set_position(editor, new_position);
    recalculate(editor);
    save_undo_action(editor);
}

static void on_edit_first(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    set_position(editor, 0);
}

static void on_edit_last(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    zm_step_x step_count = ysw_array_get_count(editor->section->steps);
    set_position(editor, step_count * 2);
}

static void on_chord_duration(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->chord_builder.frequency = item->value;
    play_chord(editor, &editor->chord_builder);
    realize_chord(editor, &editor->chord_builder);
}

static void on_chord_style(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->chord_builder.style = ysw_array_get(editor->music->chord_styles, item->value);
}

static void close_editor(ysw_editor_t *editor)
{
    ysw_event_fire_stop(editor->bus);
    ysw_app_terminate();
}

static void on_confirm_save(void *context, ysw_popup_t *popup)
{
    ysw_editor_t *editor = context;
    editor->section->tlm = editor->music->settings.clock++;
    uint32_t original_index = ysw_array_find(editor->music->sections, editor->original_section);
    if (original_index == -1) {
        ysw_array_push(editor->music->sections, editor->section);
        zm_section_free(editor->original_section);
    } else {
        zm_copy_section(editor->original_section, editor->section); // preserve section address
        ysw_staff_set_section(editor->staff, NULL);
        zm_section_free(editor->section);
    }
    zm_save_music(editor->music);
    close_editor(editor);
}

static void on_confirm_discard(void *context, ysw_popup_t *popup)
{
    ysw_editor_t *editor = context;
    close_editor(editor);
}

static void prompt_to_save_changes(ysw_editor_t *editor)
{
    ysw_string_t *s = ysw_string_create(128);
    ysw_string_printf(s, "File modified:\n%s\nSave changes?", editor->original_section->name);
    ysw_popup_config_t config = {
        .type = YSW_MSGBOX_YES_NO_CANCEL,
        .context = editor,
        .message = ysw_string_get_chars(s),
        .on_yes = on_confirm_save,
        .on_no = on_confirm_discard,
        .okay_scan_code = 5,
        .no_scan_code = 6,
        .cancel_scan_code = 7,
    };
    ysw_popup_create(&config);
    ysw_string_free(s);
}

static void on_close(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    if (editor->modified) {
        prompt_to_save_changes(editor);
    } else {
        close_editor(editor);
    }
}

static void on_insert_chord_type(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->chord_builder.type = ysw_array_get(editor->music->chord_types, item->value);
    generate_chord_style_menu(editor, on_chord_style, chord_style_template, chord_duration_menu, "Style");
}

static void on_insert_chord_pitch(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->chord_builder.root = (editor->insert_settings.octave * 12) + item->value;
    generate_chord_type_menu(editor, on_insert_chord_type, chord_type_template, chord_style_template, "Type");
}

static void on_insert_note_duration(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    zm_note_t midi_note = (editor->insert_settings.octave * 12) + editor->insert_settings.semis;
    zm_time_x duration_ticks = item->value;
    play_note(editor, midi_note, duration_ticks);
    zm_time_x duration_millis = ysw_ticks_to_millis(duration_ticks, editor->section->tempo);
    realize_note(editor, midi_note, duration_millis);
}

static void on_insert_note_pitch(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->insert_settings.semis = item->value;
}

static void on_octave(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    zm_octave_t new_octave = editor->insert_settings.octave + item->value;
    if (new_octave >= YSW_MIDI_LOWEST_OCTAVE && new_octave <= YSW_MIDI_HIGHEST_OCTAVE) {
        editor->insert_settings.octave = new_octave;
        // TODO: display a confirmation?
    }
}

static const ysw_menu_item_t insert_note_duration_menu[];

static void on_insert_note(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    generate_scale_notes_menu(editor, on_insert_note_pitch, insert_note_duration_menu, "Note");
}

static void on_insert_chord(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    generate_scale_notes_menu(editor, on_insert_chord_pitch, chord_type_template, "Chord");
}

static ysw_menu_item_t stroke_template[YSW_APP_SOFTKEY_SZ + 1];

static void on_insert_stroke_2(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    play_stroke(editor, item->value);
    realize_stroke(editor, item->value);
}

static void on_insert_stroke_1(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    generate_surface_menu(editor, on_insert_stroke_2, stroke_template, NULL, "Stroke");
}

static ysw_menu_item_t beat_template[YSW_APP_SOFTKEY_SZ + 1];

static void on_insert_beat_2(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    zm_beat_t *beat = ysw_array_get(editor->music->beats, item->value);
    play_beat(editor, beat);
    realize_beat(editor, beat);
}

static void on_insert_beat_1(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    generate_beat_menu(editor, on_insert_beat_2, beat_template, NULL, "Beat");
}

static void on_remove_beat(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    zm_step_t *step = get_step(editor);
    if (step) {
        step->rhythm.beat = NULL;
        recalculate(editor);
        save_undo_action(editor);
    }
}

static void on_remove_chord(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    zm_step_t *step = get_step(editor);
    if (step) {
        step->chord.root = 0;
        step->chord.type = NULL;
        step->chord.style = NULL;
        step->chord.duration = 0;
        step->chord.frequency = 0;
        recalculate(editor);
        save_undo_action(editor);
    }
}

static void on_remove_note(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    zm_step_t *step = get_step(editor);
    if (step) {
        step->melody.note = 0;
        step->melody.duration = 0;
        step->melody.tie = 0;
        recalculate(editor);
        save_undo_action(editor);
    }
}

static void on_remove_rest(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    zm_step_t *step = get_step(editor);
    if (step) {
        step->melody.note = 0;
        step->melody.duration = 0;
        step->melody.tie = 0;
        recalculate(editor);
        save_undo_action(editor);
    }
}

static void on_remove_stroke(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    zm_step_t *step = get_step(editor);
    if (step) {
        step->rhythm.surface = 0;
        recalculate(editor);
        save_undo_action(editor);
    }
}

// TODO: consider whether it makes sense to use the set_tie approach for the above
static void on_remove_tie(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    set_tie(editor, 0);
}

int compare_steps(const void *left, const void *right)
{
    const zm_step_t *left_step = *(zm_step_t * const *)left;
    const zm_step_t *right_step = *(zm_step_t * const *)right;
    int delta = left_step->start - right_step->start;
    return delta;
}

static void on_note_status(ysw_editor_t *editor, ysw_event_t *event)
{
    if (event->note_status.note.channel >= BACKGROUND_BASE) {
        zm_step_t needle = {
            .start = event->note_status.note.start,
        };
        ysw_array_match_t flags = YSW_ARRAY_MATCH_EXACT;
        int32_t result = ysw_array_search(editor->section->steps, &needle, compare_steps, flags);
        if (result != -1) {
            editor->position = step_index_to_position(result);
            ysw_staff_set_position(editor->staff, editor->position, YSW_STAFF_DIRECT);
        } else {
            // e.g. a stroke in a beat
        }
    }
}

static int8_t notekey_types[] = {
    +1, -1, +2, -2, +3, +4, -3, +5, -4, +6, -5, +7, +8, -6, +9, -7, +10, +11, -8, +12, -9, +13, -10, +14
};

#define NOTE_INDEX_SZ (sizeof(notekey_types) / sizeof(notekey_types[0]))

static void on_notekey_down(ysw_editor_t *editor, ysw_event_notekey_down_t *m)
{
    if (editor->harp) {
        int8_t semitone = (m->midi_note - 60) % NOTE_INDEX_SZ;
        int8_t notekey_type = notekey_types[semitone];
        if (notekey_type > 0) {
            uint32_t distance_count = ysw_array_get_count(editor->chord_type->distances);
            int8_t note_index = notekey_type - 1;
            int8_t distance_index = note_index % distance_count;
            int8_t octave = note_index / distance_count;
            int8_t distance = YSW_INT ysw_array_get(editor->chord_type->distances, distance_index);
            zm_note_t note = 36 + (octave * 12) + distance;
            // ESP_LOGD(TAG, "input=%d, note_index=%d, distance_index=%d, octave=%d, distance=%d, output=%d", m->midi_note, note_index, distance_index, octave, distance, note);
            press_note(editor, note, m->time);
        }
    } else {
        press_note(editor, m->midi_note, m->time);
        //fire_note_on(editor, MELODY_CHANNEL, m->midi_note);
    }
}

static void on_notekey_up(ysw_editor_t *editor, ysw_event_notekey_up_t *m)
{
    if (editor->harp) {
    } else {
        release_note(editor, m->midi_note, m->time, m->duration);
        //fire_note_off(editor, MELODY_CHANNEL, m->midi_note);
    }
}

static void process_event(void *context, ysw_event_t *event)
{
    ysw_editor_t *editor = context;
    switch (event->header.type) {
        case YSW_EVENT_NOTEKEY_DOWN:
            on_notekey_down(editor, &event->notekey_down);
            break;
        case YSW_EVENT_NOTEKEY_UP:
            on_notekey_up(editor, &event->notekey_up);
            break;
        case YSW_EVENT_SOFTKEY_DOWN:
            ysw_menu_on_softkey_down(editor->menu, event);
            break;
        case YSW_EVENT_SOFTKEY_UP:
            ysw_menu_on_softkey_up(editor->menu, event);
            break;
        case YSW_EVENT_SOFTKEY_PRESSED:
            ysw_menu_on_softkey_pressed(editor->menu, event);
            break;
        case YSW_EVENT_NOTE_STATUS:
            on_note_status(editor, event);
            break;
        default:
            break;
    }
}

static const ysw_menu_item_t edit_menu_2[] = {
    { YSW_R1_C2, "Pitch-", YSW_MF_STICKY, on_edit_pitch, -1, NULL },
    { YSW_R1_C3, "Pitch+", YSW_MF_STICKY, on_edit_pitch, +1, NULL },

    { YSW_R2_C1, "Note\nLength-", YSW_MF_STICKY, on_edit_length, -1, NULL },

    { YSW_R3_C1, "Note\nLength+", YSW_MF_STICKY, on_edit_length, +1, NULL },
    { YSW_R3_C4, "Left", YSW_MF_STICKY, on_left, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C3, "Hide\nMenu", YSW_MF_TOGGLE, ysw_menu_nop, 0, NULL },
    { YSW_R4_C4, "Right", YSW_MF_STICKY, on_right, 0, NULL },

    { 0, "Sound", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t edit_menu[] = {
    { YSW_R1_C1, "Sound", YSW_MF_PLUS, ysw_menu_nop, 0, edit_menu_2 },
    { YSW_R1_C2, "Drop\nAnchor", YSW_MF_STICKY, on_edit_drop_anchor, 0, NULL },
    { YSW_R1_C3, "Clear\nAnchor", YSW_MF_STICKY, on_edit_clear_anchor, 0, NULL },
    { YSW_R1_C4, "Previous", YSW_MF_STICKY, on_edit_previous, 0, NULL },

    { YSW_R2_C1, "Cut", YSW_MF_STICKY, on_edit_to_clipboard, EDIT_CUT, NULL },
    { YSW_R2_C2, "Copy", YSW_MF_STICKY, on_edit_to_clipboard, EDIT_COPY, NULL },
    { YSW_R2_C3, "Paste", YSW_MF_STICKY, on_edit_paste, 0, NULL },
    { YSW_R2_C4, "Next", YSW_MF_STICKY, on_edit_next, 0, NULL },

    { YSW_R3_C1, "First", YSW_MF_STICKY, on_edit_first, 0, NULL },
    { YSW_R3_C2, "Last", YSW_MF_STICKY, on_edit_last, 0, NULL },
    { YSW_R3_C4, "Left", YSW_MF_STICKY, on_left, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C2, "Delete", YSW_MF_STICKY, on_delete, 0, NULL },
    { YSW_R4_C3, "Hide\nMenu", YSW_MF_TOGGLE, ysw_menu_nop, 0, NULL },
    { YSW_R4_C4, "Right", YSW_MF_STICKY, on_right, 0, NULL },

    { 0, "Edit (Sticky)", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t headphone_volume_menu[] = {
    { YSW_R1_C1, "10\n(Soft)", YSW_MF_COMMAND, on_headphone_volume, 10, NULL },
    { YSW_R1_C2, "20", YSW_MF_COMMAND, on_headphone_volume, 25, NULL },
    { YSW_R1_C3, "30", YSW_MF_COMMAND, on_headphone_volume, 50, NULL },

    { YSW_R2_C1, "40", YSW_MF_COMMAND, on_headphone_volume, 75, NULL },
    { YSW_R2_C2, "50\n(Normal)", YSW_MF_COMMAND, on_headphone_volume, 100, NULL },
    { YSW_R2_C3, "60", YSW_MF_COMMAND, on_headphone_volume, 200, NULL },

    { YSW_R3_C1, "70", YSW_MF_COMMAND, on_headphone_volume, 300, NULL },
    { YSW_R3_C2, "80", YSW_MF_COMMAND, on_headphone_volume, 400, NULL },
    { YSW_R3_C3, "90\n(Loud)", YSW_MF_COMMAND, on_headphone_volume, 500, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C4, "Off", YSW_MF_COMMAND, on_headphone_volume, 0, NULL },

    { 0, "Headphone Volume", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t speaker_volume_menu[] = {
    { YSW_R1_C1, "10\n(Soft)", YSW_MF_COMMAND, on_speaker_volume, 10, NULL },
    { YSW_R1_C2, "20", YSW_MF_COMMAND, on_speaker_volume, 25, NULL },
    { YSW_R1_C3, "30", YSW_MF_COMMAND, on_speaker_volume, 50, NULL },

    { YSW_R2_C1, "40", YSW_MF_COMMAND, on_speaker_volume, 75, NULL },
    { YSW_R2_C2, "50\n(Normal)", YSW_MF_COMMAND, on_speaker_volume, 100, NULL },
    { YSW_R2_C3, "60", YSW_MF_COMMAND, on_speaker_volume, 200, NULL },

    { YSW_R3_C1, "70", YSW_MF_COMMAND, on_speaker_volume, 300, NULL },
    { YSW_R3_C2, "80", YSW_MF_COMMAND, on_speaker_volume, 400, NULL },
    { YSW_R3_C3, "90\n(Loud)", YSW_MF_COMMAND, on_speaker_volume, 500, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C4, "Off", YSW_MF_COMMAND, on_speaker_volume, 0, NULL },

    { 0, "Speaker Volume", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t synth_gain_menu[] = {
    { YSW_R1_C1, "10\n(Soft)", YSW_MF_COMMAND, on_synth_gain, 10, NULL },
    { YSW_R1_C2, "25", YSW_MF_COMMAND, on_synth_gain, 25, NULL },
    { YSW_R1_C3, "50", YSW_MF_COMMAND, on_synth_gain, 50, NULL },

    { YSW_R2_C1, "75", YSW_MF_COMMAND, on_synth_gain, 75, NULL },
    { YSW_R2_C2, "100\n(Normal)", YSW_MF_COMMAND, on_synth_gain, 100, NULL },
    { YSW_R2_C3, "200", YSW_MF_COMMAND, on_synth_gain, 200, NULL },

    { YSW_R3_C1, "300", YSW_MF_COMMAND, on_synth_gain, 300, NULL },
    { YSW_R3_C2, "400", YSW_MF_COMMAND, on_synth_gain, 400, NULL },
    { YSW_R3_C3, "500\n(Loud)", YSW_MF_COMMAND, on_synth_gain, 500, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Volume (%)", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t listen_menu[] = {
    { YSW_R1_C1, "Play", YSW_MF_COMMAND, on_play, 0, NULL },
    { YSW_R1_C2, "Pause", YSW_MF_COMMAND, ysw_menu_nop, 0, NULL },
    { YSW_R1_C3, "Resume", YSW_MF_COMMAND, ysw_menu_nop, 0, NULL },

    { YSW_R2_C1, "Stop", YSW_MF_COMMAND, on_stop, 0, NULL },
    { YSW_R2_C2, "Headphn\nVolume", YSW_MF_PLUS, ysw_menu_nop, 0, headphone_volume_menu },
    { YSW_R2_C3, "Speaker\nVolume", YSW_MF_PLUS, ysw_menu_nop, 0, speaker_volume_menu },

    { YSW_R3_C1, "Loop", YSW_MF_COMMAND, on_loop, 0, NULL },
    { YSW_R3_C3, "Synth\nGain", YSW_MF_PLUS, ysw_menu_nop, 0, synth_gain_menu },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C4, "Cycle\nDemo", YSW_MF_COMMAND, on_demo, 0, NULL },

    { 0, "Listen", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t key_minor_flat_menu[] = {
    { YSW_R1_C1, "d\n(1b)", YSW_MF_COMMAND, on_key_signature, 23, NULL },
    { YSW_R1_C2, "g\n(2b)", YSW_MF_COMMAND, on_key_signature, 24, NULL },
    { YSW_R1_C3, "c\n(3b)", YSW_MF_COMMAND, on_key_signature, 25, NULL },
    { YSW_R1_C4, "f\n(4b)", YSW_MF_COMMAND, on_key_signature, 26, NULL },

    { YSW_R2_C1, "b flat\n(5b)", YSW_MF_COMMAND, on_key_signature, 27, NULL },
    { YSW_R2_C2, "e flat\n(6b)", YSW_MF_COMMAND, on_key_signature, 28, NULL },
    { YSW_R2_C3, "a flat\n(6b)", YSW_MF_COMMAND, on_key_signature, 29, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "flat", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t key_minor_sharp_menu[] = {
    { YSW_R1_C1, "e\n(1#)", YSW_MF_COMMAND, on_key_signature, 16, NULL },
    { YSW_R1_C2, "b\n(2#)", YSW_MF_COMMAND, on_key_signature, 17, NULL },
    { YSW_R1_C3, "f sharp\n(3#)", YSW_MF_COMMAND, on_key_signature, 18, NULL },
    { YSW_R1_C4, "c sharp\n(4#)", YSW_MF_COMMAND, on_key_signature, 19, NULL },

    { YSW_R2_C1, "g sharp\n(5#)", YSW_MF_COMMAND, on_key_signature, 20, NULL },
    { YSW_R2_C2, "d sharp\n(6#)", YSW_MF_COMMAND, on_key_signature, 21, NULL },
    { YSW_R2_C3, "a sharp\n(6#)", YSW_MF_COMMAND, on_key_signature, 22, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "sharp", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t key_minor_menu[] = {
    { YSW_R1_C1, "a\nminor", YSW_MF_COMMAND, on_key_signature, 15, NULL },
    { YSW_R1_C2, "Keys w/\nSharps", YSW_MF_PLUS, ysw_menu_nop, 0, key_minor_sharp_menu },
    { YSW_R1_C3, "Keys w/\nFlats", YSW_MF_PLUS, ysw_menu_nop, 0, key_minor_flat_menu },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "minor", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t key_major_flat_menu[] = {
    { YSW_R1_C1, "F\n(1b)", YSW_MF_COMMAND, on_key_signature, 8, NULL },
    { YSW_R1_C2, "B flat\n(2b)", YSW_MF_COMMAND, on_key_signature, 9, NULL },
    { YSW_R1_C3, "E flat\n(3b)", YSW_MF_COMMAND, on_key_signature, 10, NULL },
    { YSW_R1_C4, "A flat\n(4b)", YSW_MF_COMMAND, on_key_signature, 11, NULL },

    { YSW_R2_C1, "D flat\n(5b)", YSW_MF_COMMAND, on_key_signature, 12, NULL },
    { YSW_R2_C2, "G flat\n(6b)", YSW_MF_COMMAND, on_key_signature, 13, NULL },
    { YSW_R2_C3, "C flat\n(7b)", YSW_MF_COMMAND, on_key_signature, 14, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Flat", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t key_major_sharp_menu[] = {
    { YSW_R1_C1, "G\n(1#)", YSW_MF_COMMAND, on_key_signature, 1, NULL },
    { YSW_R1_C2, "D\n(2#)", YSW_MF_COMMAND, on_key_signature, 2, NULL },
    { YSW_R1_C3, "A\n(3#)", YSW_MF_COMMAND, on_key_signature, 3, NULL },
    { YSW_R1_C4, "E\n(4#)", YSW_MF_COMMAND, on_key_signature, 4, NULL },

    { YSW_R2_C1, "B\n(5#)", YSW_MF_COMMAND, on_key_signature, 5, NULL },
    { YSW_R2_C2, "F sharp\n(6#)", YSW_MF_COMMAND, on_key_signature, 6, NULL },
    { YSW_R2_C3, "C sharp\n(7#)", YSW_MF_COMMAND, on_key_signature, 7, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Sharp", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t key_major_menu[] = {
    { YSW_R1_C1, "C\nMajor", YSW_MF_COMMAND, on_key_signature, 0, NULL },
    { YSW_R1_C2, "Keys w/\nSharps", YSW_MF_PLUS, ysw_menu_nop, 0, key_major_sharp_menu },
    { YSW_R1_C3, "Keys w/\nFlats", YSW_MF_PLUS, ysw_menu_nop, 0, key_major_flat_menu },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Major", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t key_signature_menu[] = {
    { YSW_R1_C1, "Major", YSW_MF_PLUS, ysw_menu_nop, 0, key_major_menu },
    { YSW_R1_C2, "minor", YSW_MF_PLUS, ysw_menu_nop, 0, key_minor_menu },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Key", YSW_MF_END, NULL, 0, NULL },
};

// See https://en.wikipedia.org/wiki/Time_signature

static const ysw_menu_item_t time_signature_menu[] = {
    { YSW_R1_C1, "2/2\nMarch", YSW_MF_COMMAND, on_time_signature, ZM_TIME_2_2, NULL },
    { YSW_R1_C2, "2/4\nPolka", YSW_MF_COMMAND, on_time_signature, ZM_TIME_2_4, NULL },
    { YSW_R1_C3, "3/4\nWaltz", YSW_MF_COMMAND, on_time_signature, ZM_TIME_3_4, NULL },
    { YSW_R1_C4, "4/4\nCommon", YSW_MF_COMMAND, on_time_signature, ZM_TIME_4_4, NULL },

    { YSW_R2_C1, "6/8", YSW_MF_COMMAND, on_time_signature, ZM_TIME_6_8, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Time", YSW_MF_END, NULL, 0, NULL },
};

// See https://en.wikipedia.org/wiki/Tempo

static const ysw_menu_item_t tempo_menu[] = {
    { YSW_R1_C1, "15\n(Slow)", YSW_MF_COMMAND, on_tempo, 15, NULL },
    { YSW_R1_C2, "30", YSW_MF_COMMAND, on_tempo, 30, NULL },
    { YSW_R1_C3, "45", YSW_MF_COMMAND, on_tempo, 45, NULL },
    { YSW_R1_C4, "60", YSW_MF_COMMAND, on_tempo, 60, NULL },

    { YSW_R2_C1, "70", YSW_MF_COMMAND, on_tempo, 70, NULL },
    { YSW_R2_C2, "85", YSW_MF_COMMAND, on_tempo, 85, NULL },
    { YSW_R2_C3, "100", YSW_MF_COMMAND, on_tempo, 100, NULL },
    { YSW_R2_C4, "115", YSW_MF_COMMAND, on_tempo, 115, NULL },

    { YSW_R3_C1, "135", YSW_MF_COMMAND, on_tempo, 135, NULL },
    { YSW_R3_C2, "150", YSW_MF_COMMAND, on_tempo, 150, NULL },
    { YSW_R3_C3, "165", YSW_MF_COMMAND, on_tempo, 165, NULL },
    { YSW_R3_C4, "180\n(Fast)", YSW_MF_COMMAND, on_tempo, 180, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Tempo (BPM)", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t default_note_duration_menu[] = {
    { YSW_R1_C1, "Quarter", YSW_MF_COMMAND, on_default_note_duration, ZM_QUARTER, NULL },
    { YSW_R1_C2, "Half", YSW_MF_COMMAND, on_default_note_duration, ZM_HALF, NULL },
    { YSW_R1_C3, "Whole", YSW_MF_COMMAND, on_default_note_duration, ZM_WHOLE, NULL },
    { YSW_R1_C4, "As\nPlayed", YSW_MF_COMMAND, on_default_note_duration, 0, NULL },

    { YSW_R2_C1, "Eighth", YSW_MF_COMMAND, on_default_note_duration, ZM_EIGHTH, NULL },
    { YSW_R2_C2, "Dotted\nQuarter", YSW_MF_COMMAND, on_default_note_duration, ZM_DOTTED_QUARTER, NULL },

    { YSW_R3_C1, "Sixtnth", YSW_MF_COMMAND, on_default_note_duration, ZM_SIXTEENTH, NULL },
    { YSW_R3_C3, "Dotted\nHalf", YSW_MF_COMMAND, on_default_note_duration, ZM_DOTTED_HALF, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Note", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t default_chord_frequency_menu[] = {
    { YSW_R1_C1, "1 per\nMeasure", YSW_MF_COMMAND, on_default_chord_frequency, ZM_ONE_PER_MEASURE, NULL },
    { YSW_R1_C2, "2 per\nMeasure", YSW_MF_COMMAND, on_default_chord_frequency, ZM_TWO_PER_MEASURE, NULL },
    { YSW_R1_C3, "3 per\nMeasure", YSW_MF_COMMAND, on_default_chord_frequency, ZM_THREE_PER_MEASURE, NULL },
    { YSW_R1_C4, "4 per\nMeasure", YSW_MF_COMMAND, on_default_chord_frequency, ZM_FOUR_PER_MEASURE, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Chord", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t default_drum_cadence_menu[] = {
    { YSW_R1_C1, "Sixtnth", YSW_MF_COMMAND, on_default_drum_cadence, ZM_SIXTEENTH, NULL },
    { YSW_R1_C2, "Eighth", YSW_MF_COMMAND, on_default_drum_cadence, ZM_EIGHTH, NULL },
    { YSW_R1_C3, "Quarter", YSW_MF_COMMAND, on_default_drum_cadence, ZM_QUARTER, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Drum", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t settings_duration_menu[] = {
    { YSW_R1_C1, "Note", YSW_MF_PLUS, ysw_menu_nop, 0, default_note_duration_menu },
    { YSW_R1_C2, "Chord", YSW_MF_PLUS, ysw_menu_nop, 0, default_chord_frequency_menu },
    { YSW_R1_C3, "Drum", YSW_MF_PLUS, ysw_menu_nop, 0, default_drum_cadence_menu },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Duration", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t program_category_2_menu[] = {
    { YSW_R1_C1, "Reed", YSW_MF_PLUS, on_program_2, 8, program_template },
    { YSW_R1_C2, "Pipe", YSW_MF_PLUS, on_program_2, 9, program_template },
    { YSW_R1_C3, "Synth\nLead", YSW_MF_PLUS, on_program_2, 10, program_template },
    { YSW_R1_C4, "Synth\nPad", YSW_MF_PLUS, on_program_2, 11, program_template },

    { YSW_R2_C1, "Synth\nEffects", YSW_MF_PLUS, on_program_2, 12, program_template },
    { YSW_R2_C2, "Ethnic", YSW_MF_PLUS, on_program_2, 13, program_template },
    { YSW_R2_C3, "Perc", YSW_MF_PLUS, on_program_2, 14, program_template },
    { YSW_R2_C4, "Sound\nEffects", YSW_MF_PLUS, on_program_2, 15, program_template },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "More", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t program_category_1_menu[] = {
    { YSW_R1_C1, "Piano", YSW_MF_PLUS, on_program_2, 0, program_template },
    { YSW_R1_C2, "Chrom\nPerc", YSW_MF_PLUS, on_program_2, 1, program_template },
    { YSW_R1_C3, "Organ", YSW_MF_PLUS, on_program_2, 2, program_template },
    { YSW_R1_C4, "Guitar", YSW_MF_PLUS, on_program_2, 3, program_template },

    { YSW_R2_C1, "Bass", YSW_MF_PLUS, on_program_2, 4, program_template },
    { YSW_R2_C2, "Strings", YSW_MF_PLUS, on_program_2, 5, program_template },
    { YSW_R2_C3, "Ensem\nble", YSW_MF_PLUS, on_program_2, 6, program_template },
    { YSW_R2_C4, "Brass", YSW_MF_PLUS, on_program_2, 7, program_template },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C4, "More", YSW_MF_PLUS, ysw_menu_nop, 0, program_category_2_menu },

    { 0, "Category", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t settings_menu[] = {
    { YSW_R1_C1, "Key\nSig", YSW_MF_PLUS, ysw_menu_nop, 0, key_signature_menu },
    { YSW_R1_C2, "Time\nSig", YSW_MF_PLUS, ysw_menu_nop, 0, time_signature_menu },
    { YSW_R1_C3, "Tempo\n(BPM)", YSW_MF_PLUS, ysw_menu_nop, 0, tempo_menu },
    { YSW_R1_C4, "Duration", YSW_MF_PLUS, ysw_menu_nop, 0, settings_duration_menu },

    { YSW_R2_C1, "Melody\nProgram", YSW_MF_PLUS, on_program_1, 0, program_category_1_menu },
    { YSW_R2_C2, "Chord\nProgram", YSW_MF_PLUS, on_program_1, 1, program_category_1_menu },

    { YSW_R3_C1, "Chord\nType", YSW_MF_PLUS, on_settings_chord_type_1, 0, chord_type_template },
    { YSW_R3_C2, "Chord\nStyle", YSW_MF_PLUS, on_settings_chord_style_1, 0, chord_style_template },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Settings", YSW_MF_END, NULL, 0, NULL },
};

#if 0
static const ysw_menu_item_t menu_template[] = {
    { YSW_R1_C1, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C2, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C3, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C4, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R2_C1, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R2_C2, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R2_C3, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R2_C4, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R3_C1, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R3_C2, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R3_C3, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R3_C4, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C2, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R4_C3, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R4_C4, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { 0, "Menu Name", YSW_MF_END, NULL, 0, NULL },
};
#endif

static const ysw_menu_item_t insert_note_duration_menu[] = {
    { YSW_R1_C1, "Quarter", YSW_MF_COMMAND, on_insert_note_duration, ZM_QUARTER, NULL },
    { YSW_R1_C2, "Half", YSW_MF_COMMAND, on_insert_note_duration, ZM_HALF, NULL },
    { YSW_R1_C3, "Whole", YSW_MF_COMMAND, on_insert_note_duration, ZM_WHOLE, NULL },

    { YSW_R2_C1, "Eighth", YSW_MF_COMMAND, on_insert_note_duration, ZM_EIGHTH, NULL },
    { YSW_R2_C2, "Dotted\nQuarter", YSW_MF_COMMAND, on_insert_note_duration, ZM_DOTTED_QUARTER, NULL },

    { YSW_R3_C1, "Sixtnth", YSW_MF_COMMAND, on_insert_note_duration, ZM_SIXTEENTH, NULL },
    { YSW_R3_C3, "Dotted\nHalf", YSW_MF_COMMAND, on_insert_note_duration, ZM_DOTTED_HALF, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Duration", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t chord_duration_menu[] = {
    { YSW_R1_C1, "1 per\nMeasure", YSW_MF_COMMAND, on_chord_duration, ZM_ONE_PER_MEASURE, NULL },
    { YSW_R1_C2, "2 per\nMeasure", YSW_MF_COMMAND, on_chord_duration, ZM_TWO_PER_MEASURE, NULL },
    { YSW_R1_C3, "3 per\nMeasure", YSW_MF_COMMAND, on_chord_duration, ZM_THREE_PER_MEASURE, NULL },
    { YSW_R1_C4, "4 per\nMeasure", YSW_MF_COMMAND, on_chord_duration, ZM_FOUR_PER_MEASURE, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Duration", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t rest_duration_menu[] = {
    { YSW_R1_C1, "Quarter", YSW_MF_COMMAND, on_rest_duration, ZM_QUARTER, NULL },
    { YSW_R1_C2, "Half", YSW_MF_COMMAND, on_rest_duration, ZM_HALF, NULL },
    { YSW_R1_C3, "Whole", YSW_MF_COMMAND, on_rest_duration, ZM_WHOLE, NULL },

    { YSW_R2_C1, "Eighth", YSW_MF_COMMAND, on_rest_duration, ZM_EIGHTH, NULL },

    { YSW_R3_C1, "Sixtnth", YSW_MF_COMMAND, on_rest_duration, ZM_SIXTEENTH, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Rest", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t insert_menu[] = {
    { YSW_R1_C1, "Note", YSW_MF_PLUS, on_insert_note, 0, diatonic_template },
    { YSW_R1_C2, "Chord", YSW_MF_PLUS, on_insert_chord, 0, diatonic_template },
    { YSW_R1_C3, "Stroke", YSW_MF_PLUS, on_insert_stroke_1, 0, stroke_template },
    { YSW_R1_C4, "Beat", YSW_MF_PLUS, on_insert_beat_1, 0, beat_template },

    { YSW_R2_C1, "Rest", YSW_MF_PLUS, ysw_menu_nop, 0, rest_duration_menu },

    { YSW_R3_C1, "Tie", YSW_MF_COMMAND, on_insert_tie, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Insert", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t remove_menu[] = {
    { YSW_R1_C1, "Note", YSW_MF_COMMAND, on_remove_note, 0, NULL },
    { YSW_R1_C2, "Chord", YSW_MF_COMMAND, on_remove_chord, 0, NULL },
    { YSW_R1_C3, "Stroke", YSW_MF_COMMAND, on_remove_stroke, 0, NULL },
    { YSW_R1_C4, "Beat", YSW_MF_COMMAND, on_remove_beat, 0, NULL },

    { YSW_R2_C1, "Rest", YSW_MF_COMMAND, on_remove_rest, 0, NULL },

    { YSW_R3_C1, "Tie", YSW_MF_COMMAND, on_remove_tie, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Remove", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t input_mode_menu[] = {
    { YSW_R1_C1, "Note", YSW_MF_COMMAND, on_mode_melody, 0, NULL },
    { YSW_R1_C2, "Chord", YSW_MF_COMMAND, on_mode_chord, 0, NULL },
    { YSW_R1_C3, "Rhythm", YSW_MF_COMMAND, on_mode_rhythm, 0, NULL },

    { YSW_R2_C1, "Harp", YSW_MF_COMMAND, on_mode_harp, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Input Mode", YSW_MF_END, NULL, 0, NULL },
};

const ysw_menu_item_t editor_menu[] = {
    { YSW_R1_C1, "Input\nMode", YSW_MF_PLUS, ysw_menu_nop, 0, input_mode_menu },
    { YSW_R1_C2, "Insert", YSW_MF_PLUS, ysw_menu_nop, 0, insert_menu },
    { YSW_R1_C3, "Remove", YSW_MF_PLUS, ysw_menu_nop, 0, remove_menu },
    { YSW_R1_C4, "Up", YSW_MF_COMMAND, on_edit_pitch, +1, NULL },

    { YSW_R2_C1, "Listen", YSW_MF_PLUS, ysw_menu_nop, 0, listen_menu },
    { YSW_R2_C2, "Edit", YSW_MF_PLUS, ysw_menu_nop, 0, edit_menu },
    { YSW_R2_C4, "Down", YSW_MF_COMMAND, on_edit_pitch, -1, NULL },

    { YSW_R3_C1, "Close", YSW_MF_MINUS, on_close, 0, NULL },
    { YSW_R3_C3, "Settings", YSW_MF_PLUS, ysw_menu_nop, 0, settings_menu },
    { YSW_R3_C4, "Left", YSW_MF_COMMAND, on_left, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C2, "Delete", YSW_MF_COMMAND, on_delete, 0, NULL },
    { YSW_R4_C3, "Hide\nMenu", YSW_MF_TOGGLE, ysw_menu_nop, 0, NULL },
    { YSW_R4_C4, "Right", YSW_MF_COMMAND, on_right, 0, NULL },

    { YSW_BUTTON_1, "Left", YSW_MF_COMMAND, on_left, 0, NULL },
    { YSW_BUTTON_2, "Right", YSW_MF_COMMAND, on_right, 0, NULL },
    { YSW_BUTTON_3, "Delete", YSW_MF_COMMAND, on_delete, 0, NULL },
    { YSW_BUTTON_4, "Up", YSW_MF_COMMAND, on_edit_pitch, +1, NULL },
    { YSW_BUTTON_5, "Down", YSW_MF_COMMAND, on_edit_pitch, -1, NULL },

    { 0, "Music", YSW_MF_END, NULL, 0, NULL },
};

void ysw_editor_edit_section(ysw_bus_t *bus, zm_music_t *music, zm_section_t *section)
{
    assert(music);
    assert(ysw_array_get_count(music->chord_types) > DEFAULT_CHORD_TYPE);
    assert(ysw_array_get_count(music->chord_styles));

    ysw_editor_t *editor = ysw_heap_allocate(sizeof(ysw_editor_t));

    editor->bus = bus;
    editor->music = music;
    editor->original_section = section;
    editor->section = zm_create_duplicate_section(section);

    if (!clipboard) {
        clipboard = ysw_array_create(8);
    }

    editor->chord_type = ysw_array_get(music->chord_types, DEFAULT_CHORD_TYPE);
    editor->chord_style = ysw_array_get(music->chord_styles, DEFAULT_CHORD_STYLE);
    editor->chord_frequency = ZM_ONE_PER_MEASURE;
    editor->beat = ysw_array_get(music->beats, DEFAULT_BEAT);
    editor->duration = ZM_AS_PLAYED;
    editor->drum_cadence = ZM_EIGHTH;

    editor->insert = true;
    editor->modified = section->tlm ? false : true; // set modified for new file
    editor->position = 0;
    editor->mode = YSW_EDITOR_MODE_MELODY;
    editor->insert_settings.octave = YSW_MIDI_MIDDLE_OCTAVE;

    zm_recalculate_section(editor->section);

    editor->container = lv_obj_create(lv_scr_act(), NULL);
    assert(editor->container);

    ysw_style_editor(editor->container);

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

    ysw_staff_set_section(editor->staff, editor->section);
    ysw_footer_set_key(editor->footer, editor->section->key);
    ysw_footer_set_time(editor->footer, editor->section->time);
    ysw_footer_set_tempo(editor->footer, editor->section->tempo);
    fire_program_change(editor, MELODY_CHANNEL, editor->section->melody_program);
    fire_program_change(editor, CHORD_CHANNEL, editor->section->chord_program);
    fire_bank_select(editor, RHYTHM_CHANNEL, YSW_MIDI_DRUM_BANK);
    fire_bank_select(editor, BACKGROUND_RHYTHM, YSW_MIDI_DRUM_BANK);
    display_mode(editor); // mode displays program

    editor->menu = ysw_menu_create(bus, editor_menu, ysw_app_softkey_map, editor);
    ysw_menu_add_glass(editor->menu, editor->container);

    editor->queue = ysw_app_create_queue();
    ysw_bus_subscribe(bus, YSW_ORIGIN_NOTE, editor->queue);
    ysw_bus_subscribe(bus, YSW_ORIGIN_SOFTKEY, editor->queue);
    ysw_bus_subscribe(bus, YSW_ORIGIN_SEQUENCER, editor->queue);

    ysw_app_handle_events(editor->queue, process_event, editor);

    ysw_bus_unsubscribe(bus, YSW_ORIGIN_NOTE, editor->queue);
    ysw_bus_unsubscribe(bus, YSW_ORIGIN_SOFTKEY, editor->queue);
    ysw_bus_unsubscribe(bus, YSW_ORIGIN_SEQUENCER, editor->queue);
    ysw_bus_delete_queue(bus, editor->queue);

    ysw_menu_free(editor->menu);
    lv_obj_del(editor->container);
    ysw_heap_free(editor);
}

