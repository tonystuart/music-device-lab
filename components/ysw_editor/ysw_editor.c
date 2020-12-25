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

#define TAG "YSW_EDITOR"

#define DEFAULT_CHORD_TYPE 2 // TODO: make "major" 0 default or search by name
#define DEFAULT_CHORD_STYLE 2 // TODO: make "stacked" 0 default or search by name
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
    ysw_menu_t *menu;
    QueueHandle_t queue;
    ysw_app_control_t control;

    zm_music_t *music;
    zm_section_t *section;
    zm_chord_type_t *chord_type;
    zm_chord_style_t *chord_style;
    zm_chord_frequency_t chord_frequency;
    zm_beat_t *beat;
    zm_duration_t duration;
    zm_duration_t drum_cadence;

    zm_time_x down_at;
    zm_time_x up_at;
    zm_time_x delta;

    bool loop;
    bool insert;
    uint32_t position;
    ysw_editor_mode_t mode;

    lv_obj_t *container;
    lv_obj_t *header;
    lv_obj_t *staff;
    lv_obj_t *footer;

    ysw_popup_t *popup;

    zm_chord_t chord_builder;

} ysw_editor_t;

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

static inline bool is_step_position(ysw_editor_t *editor)
{
    return editor->position % 2 == 1;
}

static inline bool is_space_position(ysw_editor_t *editor)
{
    return editor->position % 2 == 0;
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

static void update_tlm(ysw_editor_t *editor)
{
    editor->section->tlm = editor->music->settings.clock++;
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
    if (is_step_position(editor)) {
        zm_step_x step_index = editor->position / 2;
        zm_step_t *step = ysw_array_get(editor->section->steps, step_index);
        play_step(editor, step);
    }
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

static void display_program(ysw_editor_t *editor)
{
    const char *value = "";
    if (editor->mode == YSW_EDITOR_MODE_MELODY) {
        value = editor->section->melody_program->name;
    } else if (editor->mode == YSW_EDITOR_MODE_CHORD) {
        value = editor->section->chord_program->name;
    } else if (editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        value = editor->section->rhythm_program->name;
    }
    ysw_header_set_program(editor->header, value);
}

static void display_melody_mode(ysw_editor_t *editor)
{
    // 1. on a step - show note or rest
    // 2. between steps - show previous (to left) note or rest
    // 3. no steps - show blank
    char value[32];
    zm_step_x step_count = ysw_array_get_count(editor->section->steps);
    if (editor->position > 0) {
        zm_step_x step_index = editor->position / 2;
        if (step_index >= step_count) {
            step_index = step_count - 1;
        }
        zm_step_t *step = ysw_array_get(editor->section->steps, step_index);
        ESP_LOGD(TAG, "step_index=%d, start=%d", step_index, step->start);
        uint32_t millis = ysw_ticks_to_millis(step->melody.duration, editor->section->tempo);
        zm_note_t note = step->melody.note;
        if (note) {
            uint8_t octave = (note / 12) - 1;
            const char *name = zm_get_note_name(note);
            snprintf(value, sizeof(value), "%s%d (%d ms)", name, octave, millis);
        } else {
            snprintf(value, sizeof(value), "Rest (%d ms)", millis);
        }
    } else {
        snprintf(value, sizeof(value), "%s", editor->section->name);
    }
    ysw_header_set_mode(editor->header, modes[editor->mode], value);
}

static void display_chord_mode(ysw_editor_t *editor)
{
    // 1. on a step with a chord value - show chord value, chord_type, chord_style
    // 2. on a step without a chord value - show chord_type, chord_style
    // 3. between steps - show chord_type, chord_style
    // 4. no steps - show chord_type, chord_style
    char value[32] = {};
    zm_step_t *step = NULL;
    if (is_step_position(editor)) {
        step = ysw_array_get(editor->section->steps, editor->position / 2);
    }
    if (step && step->chord.root) {
        snprintf(value, sizeof(value), "%s %s %s",
                zm_get_note_name(step->chord.root),
                step->chord.type->name,
                step->chord.style->name);
    } else {
        snprintf(value, sizeof(value), "%s %s",
                editor->chord_type->name,
                editor->chord_style->name);
    }
    ysw_header_set_mode(editor->header, modes[editor->mode], value);
}

static void display_rhythm_mode(ysw_editor_t *editor)
{
    char value[32] = {};
    zm_step_t *step = NULL;
    if (is_step_position(editor)) {
        step = ysw_array_get(editor->section->steps, editor->position / 2);
    }
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
        editor->section->melody_program = next_program(editor->music->programs,
                editor->section->melody_program, ZM_PROGRAM_NOTE);
        fire_program_change(editor, editor->section->melody_program, MELODY_CHANNEL);
    } else if (editor->mode == YSW_EDITOR_MODE_CHORD) {
        editor->section->chord_program = next_program(editor->music->programs,
                editor->section->chord_program, ZM_PROGRAM_NOTE);
        fire_program_change(editor, editor->section->chord_program, CHORD_CHANNEL);
    } else if (editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        editor->section->rhythm_program = next_program(editor->music->programs,
                editor->section->rhythm_program, ZM_PROGRAM_BEAT);
        fire_program_change(editor, editor->section->rhythm_program, RHYTHM_CHANNEL);
    }
    display_program(editor);
    play_position(editor);
}

static void set_key_signature(ysw_editor_t *editor, zm_key_signature_x key)
{
    editor->section->key = key;
    ysw_footer_set_key(editor->footer, editor->section->key);
    ysw_staff_invalidate(editor->staff);
    update_tlm(editor);
}

static void set_time_signature(ysw_editor_t *editor, zm_time_signature_x time)
{
    editor->section->time = time;
    ysw_footer_set_time(editor->footer, editor->section->time);
    recalculate(editor);
    update_tlm(editor);
}

static void set_tempo(ysw_editor_t *editor, zm_tempo_x tempo)
{
    editor->section->tempo = tempo;
    ysw_footer_set_tempo(editor->footer, editor->section->tempo);
    display_mode(editor);
    update_tlm(editor);
}

static void set_default_note_duration(ysw_editor_t *editor, zm_duration_t duration)
{
    zm_step_t *step = NULL;
    if (is_step_position(editor) && editor->duration != ZM_AS_PLAYED) {
        zm_step_x step_index = editor->position / 2;
        step = ysw_array_get(editor->section->steps, step_index);
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
        update_tlm(editor);
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
    assert(false);
}

static void cycle_chord_type(ysw_editor_t *editor)
{
    zm_step_t *step = NULL;
    if (is_step_position(editor)) {
        zm_step_x step_index = editor->position / 2;
        step = ysw_array_get(editor->section->steps, step_index);
    }
    if (!step || !step->chord.root || step->chord.type == editor->chord_type) {
        zm_chord_type_x previous = ysw_array_find(editor->music->chord_types, editor->chord_type);
        zm_chord_type_x chord_type_count = ysw_array_get_count(editor->music->chord_types);
        zm_chord_type_x next = (previous + 1) % chord_type_count;
        editor->chord_type = ysw_array_get(editor->music->chord_types, next);
        editor->chord_style = find_matching_style(editor, false);
    }
    if (step) {
        step->chord.type = editor->chord_type;
        step->chord.style = editor->chord_style;
        ysw_staff_invalidate(editor->staff);
        update_tlm(editor);
    }
    display_mode(editor);
    play_position(editor);
}

static void cycle_style(ysw_editor_t *editor)
{
    zm_step_t *step = NULL;
    if (is_step_position(editor)) {
        zm_step_x step_index = editor->position / 2;
        step = ysw_array_get(editor->section->steps, step_index);
    }
    if (!step || !step->chord.root || step->chord.style == editor->chord_style) {
        editor->chord_style = find_matching_style(editor, true);
    }
    if (step) {
        step->chord.style = editor->chord_style;
    }
    display_mode(editor);
    play_position(editor);
}

static void cycle_beat(ysw_editor_t *editor)
{
    zm_step_t *step = NULL;
    if (is_step_position(editor)) {
        zm_step_x step_index = editor->position / 2;
        step = ysw_array_get(editor->section->steps, step_index);
    }
    if (!step || !step->rhythm.beat || step->rhythm.beat == editor->beat) {
        zm_beat_x previous = ysw_array_find(editor->music->beats, editor->beat);
        zm_beat_x beat_count = ysw_array_get_count(editor->music->beats);
        zm_beat_x next = (previous + 1) % beat_count;
        editor->beat = ysw_array_get(editor->music->beats, next);
    }
    if (step) {
        step->rhythm.beat = editor->beat;
        ysw_staff_invalidate(editor->staff);
        update_tlm(editor);
    }
    display_mode(editor);
    play_position(editor);
}

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
    editor->position = min((step_index * 2) + 2, step_count * 2);
#else
    // insert when starting on space, overtype when starting on step
    editor->position = min(editor->position + 2, step_count * 2);
#endif
    ysw_staff_set_position(editor->staff, editor->position);
    display_mode(editor);
    recalculate(editor);
    update_tlm(editor);
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

static void realize_stroke(ysw_editor_t *editor, zm_note_t midi_note)
{
    zm_step_x step_index;
    zm_step_t *step = realize_step(editor, &step_index, 0);

    step->rhythm.surface = midi_note;
    step->rhythm.cadence = editor->drum_cadence;

    finalize_step(editor, step_index);
}

static void realize_beat(ysw_editor_t *editor)
{
    zm_step_x step_index;
    zm_step_t *step = realize_step(editor, &step_index, 0);

    step->rhythm.beat = editor->beat;

    finalize_step(editor, step_index);
}

static void process_delete(ysw_editor_t *editor)
{
    zm_step_x step_index = editor->position / 2;
    zm_step_x step_count = ysw_array_get_count(editor->section->steps);
    if (step_count > 0 && step_index == step_count) {
        // On space following last step in section, delete previous step
        step_index--;
    }
    if (step_index < step_count) {
        zm_step_t *step = ysw_array_remove(editor->section->steps, step_index);
        ysw_heap_free(step);
        step_count--;
        if (step_index == step_count) {
            if (step_count) {
                editor->position -= 2;
            } else {
                editor->position = 0;
            }
        }
        ysw_staff_set_position(editor->staff, editor->position);
        display_mode(editor);
        recalculate(editor);
        update_tlm(editor);
    }
}

static void process_left(ysw_editor_t *editor, uint8_t move_amount)
{
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
    if (new_position <= ysw_array_get_count(editor->section->steps) * 2) {
        editor->position = new_position;
        play_position(editor);
        ysw_staff_set_position(editor->staff, editor->position);
        display_mode(editor);
    }
}

static void process_up(ysw_editor_t *editor)
{
    if (is_step_position(editor)) {
        zm_step_x step_index = editor->position / 2;
        zm_step_t *step = ysw_array_get(editor->section->steps, step_index);
        if (editor->mode == YSW_EDITOR_MODE_MELODY) {
            if (step->melody.note && step->melody.note < 83) {
                step->melody.note++;
            }
        } else if (editor->mode == YSW_EDITOR_MODE_CHORD) {
            if (step->chord.root && step->chord.root < 83) {
                step->chord.root++;
            }
        } else if (editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        }
        play_position(editor);
        display_mode(editor);
        update_tlm(editor);
    }
}

static void process_down(ysw_editor_t *editor)
{
    if (is_step_position(editor)) {
        zm_step_x step_index = editor->position / 2;
        zm_step_t *step = ysw_array_get(editor->section->steps, step_index);
        if (editor->mode == YSW_EDITOR_MODE_MELODY) {
            if (step->melody.note && step->melody.note > 60) {
                step->melody.note--;
            }
        } else if (editor->mode == YSW_EDITOR_MODE_CHORD) {
            if (step->chord.root && step->chord.root > 60) {
                step->chord.root--;
            }
        } else if (editor->mode == YSW_EDITOR_MODE_RHYTHM) {
        }
        play_position(editor);
        display_mode(editor);
        update_tlm(editor);
    }
}

static void on_melody_mode(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->mode = YSW_EDITOR_MODE_MELODY;
    display_mode(editor);
}

static void on_chord_mode(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->mode = YSW_EDITOR_MODE_CHORD;
    display_mode(editor);
}

static void on_rhythm_mode(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->mode = YSW_EDITOR_MODE_RHYTHM;
    display_mode(editor);
}

static void on_program(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    cycle_program(editor);
}

static void on_chord_type_x(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    cycle_chord_type(editor);
}

static void on_style(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    cycle_style(editor);
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

static void set_tie(ysw_editor_t *editor, zm_tie_x tie)
{
    if (is_step_position(editor)) {
        zm_step_x step_index = editor->position / 2;
        zm_step_t *step = ysw_array_get(editor->section->steps, step_index);
        if (step->melody.note) {
            step->melody.tie = tie;
            ysw_staff_invalidate(editor->staff);
            update_tlm(editor);
        }
    }
}

static void on_insert_tie(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    set_tie(editor, 1);
}

static void on_delete_tie(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    set_tie(editor, 0);
}

static void on_rest_duration(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
}

static void on_cycle_beat(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    cycle_beat(editor);
}

static void on_insert_beat(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    realize_beat(editor);
}

static void on_delete(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    process_delete(editor);
}

static void on_play(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    ysw_array_t *notes = zm_render_section(editor->music, editor->section, BACKGROUND_BASE);
    ysw_event_fire_play(editor->bus, notes, editor->section->tempo);
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
    process_left(editor, 1);
}

static void on_right(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    process_right(editor, 1);
}

static void on_previous(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    process_left(editor, 2);
}

static void on_next(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    process_right(editor, 2);
}

static void on_up(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    process_up(editor);
}

static void on_down(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
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

static void press_note(ysw_editor_t *editor, zm_note_t midi_note, uint32_t down_at)
{
    if (editor->mode == YSW_EDITOR_MODE_MELODY) {
        editor->down_at = down_at;
        editor->delta = editor->down_at - editor->up_at;
        if (midi_note) {
            fire_note_on(editor, midi_note, MELODY_CHANNEL);
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
            fire_note_on(editor, midi_note, RHYTHM_CHANNEL);
        }
    }
}

static void release_note(ysw_editor_t *editor, zm_note_t midi_note, uint32_t up_at, uint32_t duration)
{
    if (editor->mode == YSW_EDITOR_MODE_MELODY) {
        editor->up_at = up_at;
        if (midi_note) {
            fire_note_off(editor, midi_note, MELODY_CHANNEL);
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
            fire_note_off(editor, midi_note, RHYTHM_CHANNEL);
        }
        realize_stroke(editor, midi_note);
    }
}

static void on_note(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    if (event->header.type == YSW_EVENT_KEY_DOWN) {
        press_note(editor, item->value, event->key_down.time);
    } else if (event->header.type == YSW_EVENT_KEY_UP) {
        release_note(editor, item->value, event->key_up.time, event->key_up.duration);
    }
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

static void on_close(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->control = YSW_APP_TERMINATE;
}

#define YSW_APP_SOFTKEY_SZ 16

static const uint8_t scan_code_map[] = {
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

static ysw_menu_item_t chord_style_menu[YSW_APP_SOFTKEY_SZ + 1];

static const ysw_menu_item_t chord_duration_menu[]; // forward declaration

static void generate_chord_style_menu(ysw_editor_t *editor)
{
    zm_distance_x distance_count = ysw_array_get_count(editor->chord_builder.type->distances);
    zm_chord_style_x style_count = ysw_array_get_count(editor->music->chord_styles);
    // TODO: create multiple chord style menus if necessary
    bool full = false;
    zm_chord_style_x softkey_index = 0;
    ysw_menu_item_t *p = chord_style_menu;
    for (zm_chord_style_x i = 0; i < style_count && !full; i++) {
        zm_chord_style_t *style = ysw_array_get(editor->music->chord_styles, i);
        if (style->distance_count == distance_count) {
            p->scan_code = scan_code_map[softkey_index];
            p->name = style->label;
            p->flags = YSW_MF_PLUS;
            p->cb = on_chord_style;
            p->value = i;
            p->submenu = chord_duration_menu;
            p++;
            if (++softkey_index == 12) {
                full = true;
            }
        }
    }

    p->scan_code = YSW_R4_C1;
    p->name = "Back";
    p->flags = YSW_MF_MINUS;
    p->cb = ysw_menu_nop;
    p->value = 0;
    p->submenu = NULL;
    p++;

    p->name = "Style";
    p->flags = YSW_MF_END;
}

static void on_chord_type(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->chord_builder.type = ysw_array_get(editor->music->chord_types, item->value);
    generate_chord_style_menu(editor);
}

static ysw_menu_item_t chord_type_menu[YSW_APP_SOFTKEY_SZ + 1];

static void generate_chord_type_menu(ysw_editor_t *editor)
{
    zm_chord_type_x count = ysw_array_get_count(editor->music->chord_types);
    // TODO: use multiple chord type menus if necessary
    count = min(count, 12);
    ysw_menu_item_t *p = chord_type_menu;
    for (zm_chord_type_x i = 0; i < count; i++) {
        zm_chord_type_t *chord_type = ysw_array_get(editor->music->chord_types, i);
        p->scan_code = scan_code_map[i];
        p->name = chord_type->label;
        p->flags = YSW_MF_PLUS;
        p->cb = on_chord_type;
        p->value = i;
        p->submenu = chord_style_menu;
        p++;
    }

    p->scan_code = YSW_R4_C1;
    p->name = "Back";
    p->flags = YSW_MF_MINUS;
    p->cb = ysw_menu_nop;
    p->value = 0;
    p->submenu = NULL;
    p++;

    p->name = "Type";
    p->flags = YSW_MF_END;
}

static void on_chord_note(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    editor->chord_builder.root = item->value;
    generate_chord_type_menu(editor);
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
            ysw_staff_set_position(editor->staff, editor->position);
        } else {
            // e.g. a stroke in a beat
        }
    }
}

static void on_note_length(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_editor_t *editor = menu->context;
    int32_t direction = item->value;
    ESP_LOGD(TAG, "direction=%d", direction);
    zm_step_t *step = get_step(editor);
    if (step) {
        step->melody.duration = zm_get_next_dotted_duration(step->melody.duration, direction);
        play_position(editor);
        display_mode(editor);
        recalculate(editor);
        update_tlm(editor);
    }
}

static ysw_app_control_t process_event(void *context, ysw_event_t *event)
{
    ysw_editor_t *editor = context;
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
        default:
            break;
    }
    return editor->control;
}

// Layout of keycodes generated by keyboard:
//
//    0,  1,      2,  3,  4,      5,  6,  7,  8,
//  9, 10, 11, 12, 13, 14, 15,   16, 17, 18, 19,
//   20, 21,     22, 23, 24,     25, 26, 27, 28,
// 29, 30, 31, 32, 33, 34, 35,   36, 37, 38, 39,

// Actions that pop a menu (e.g. COMMAND_POP) must be performed on UP, not DOWN or PRESS.

static const ysw_menu_item_t melody_menu[] = {
    { 5, "Tie\nNotes", YSW_MF_COMMAND, on_insert_tie, 0, NULL },
    { 6, "Insert\nRest", YSW_MF_COMMAND, on_note, 0, NULL },
    { 7, "Melody\nSound", YSW_MF_COMMAND, on_program, 0, NULL },

    { 16, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { 17, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { 18, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { 25, "Note\nLength-", YSW_MF_COMMAND, on_note_length, -1, NULL },
    { 26, "Note\nLength+", YSW_MF_COMMAND, on_note_length, +1, NULL },
    { 27, "Clear\nNote", YSW_MF_COMMAND, ysw_menu_nop, 0, NULL },

    { 36, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Melody", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t chord_menu[] = {
    { 5, "Chord\nType", YSW_MF_COMMAND, on_chord_type_x, 0, NULL },
    { 6, "Chord\nStyle", YSW_MF_COMMAND, on_style, 0, NULL },
    { 7, "Chord\nSound", YSW_MF_COMMAND, on_program, 0, NULL },

    { 16, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { 17, "Apply\nQ&S", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { 18, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { 25, " ", YSW_MF_COMMAND, ysw_menu_nop, 0, NULL },
    { 26, " ", YSW_MF_COMMAND, ysw_menu_nop, 0, NULL },
    { 27, "Clear\nChord", YSW_MF_COMMAND, ysw_menu_nop, 0, NULL },

    { 36, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Chord", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t rhythm_menu[] = {
    { 5, "Cycle\nBeat", YSW_MF_COMMAND, on_cycle_beat, 0, NULL },
    { 6, "Insert\nBeat", YSW_MF_COMMAND, on_insert_beat, 0, NULL },
    { 7, "Rhythm\nSound", YSW_MF_COMMAND, on_program, 0, NULL },

    { 16, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { 17, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { 18, "Clear\nStroke", YSW_MF_COMMAND, ysw_menu_nop, 0, NULL },

    { 25, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { 26, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { 27, "Clear\nBeat", YSW_MF_COMMAND, ysw_menu_nop, 0, NULL },

    { 36, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Rhythm", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t edit_menu[] = {
    { 5, "Select\nLeft", YSW_MF_COMMAND, ysw_menu_nop, 0, NULL },
    { 6, "Select\nRight", YSW_MF_COMMAND, ysw_menu_nop, 0, NULL },
    { 7, "Select\nAll", YSW_MF_COMMAND, ysw_menu_nop, 0, NULL },
    { 8, "Trans\npose+", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { 16, "Cut", YSW_MF_COMMAND, ysw_menu_nop, 0, NULL },
    { 17, "Copy", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { 18, "Paste", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { 19, "Trans\npose-", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { 25, "First", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { 26, "Next", YSW_MF_COMMAND, on_next, 0, NULL },
    { 27, "Previous", YSW_MF_COMMAND, on_previous, 0, NULL },

    { 36, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { 37, "Delete", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { 0, "Edit", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t listen_menu[] = {
    { 5, "Play", YSW_MF_COMMAND, on_play, 0, NULL },
    { 6, "Pause", YSW_MF_COMMAND, ysw_menu_nop, 0, NULL },
    { 7, "Resume", YSW_MF_COMMAND, ysw_menu_nop, 0, NULL },

    { 16, "Stop", YSW_MF_COMMAND, on_stop, 0, NULL },
    { 17, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { 18, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { 25, "Loop", YSW_MF_COMMAND, on_loop, 0, NULL },
    { 26, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { 27, "Cycle\nDemo", YSW_MF_COMMAND, on_demo, 0, NULL },

    { 36, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

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

static const ysw_menu_item_t settings_menu[] = {
    { YSW_R1_C1, "Key\nSig", YSW_MF_PLUS, ysw_menu_nop, 0, key_signature_menu },
    { YSW_R1_C2, "Time\nSig", YSW_MF_PLUS, ysw_menu_nop, 0, time_signature_menu },
    { YSW_R1_C3, "Tempo\n(BPM)", YSW_MF_PLUS, ysw_menu_nop, 0, tempo_menu },
    { YSW_R1_C4, "Duration", YSW_MF_PLUS, ysw_menu_nop, 0, settings_duration_menu },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Settings", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t preferences_menu[] = {
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

    { 0, "My Prefs", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t insert_note_duration_menu[] = {
    { YSW_R1_C1, "Quarter", YSW_MF_NOP, ysw_menu_nop, ZM_QUARTER, NULL },
    { YSW_R1_C2, "Half", YSW_MF_NOP, ysw_menu_nop, ZM_HALF, NULL },
    { YSW_R1_C3, "Whole", YSW_MF_NOP, ysw_menu_nop, ZM_WHOLE, NULL },

    { YSW_R2_C1, "Eighth", YSW_MF_NOP, ysw_menu_nop, ZM_EIGHTH, NULL },
    { YSW_R2_C2, "Dotted\nQuarter", YSW_MF_NOP, ysw_menu_nop, ZM_DOTTED_QUARTER, NULL },

    { YSW_R3_C1, "Sixtnth", YSW_MF_NOP, ysw_menu_nop, ZM_SIXTEENTH, NULL },
    { YSW_R3_C3, "Dotted\nHalf", YSW_MF_NOP, ysw_menu_nop, ZM_DOTTED_HALF, NULL },

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

// See https://en.wikipedia.org/wiki/Octave#Octave_of_a_pitch

// TODO: generate based on diatonic notes of current key
// TODO: add menu_items for keyboard keys

static const ysw_menu_item_t chord_octave4_menu[] = {
    { YSW_R1_C1, "C", YSW_MF_PLUS, on_chord_note, 60, chord_type_menu },
    { YSW_R1_C2, "D", YSW_MF_PLUS, on_chord_note, 62, chord_type_menu },
    { YSW_R1_C3, "E", YSW_MF_PLUS, on_chord_note, 64, chord_type_menu },
    { YSW_R1_C4, "F", YSW_MF_PLUS, on_chord_note, 65, chord_type_menu },

    { YSW_R2_C1, "G", YSW_MF_PLUS, on_chord_note, 67, chord_type_menu },
    { YSW_R2_C2, "A", YSW_MF_PLUS, on_chord_note, 69, chord_type_menu },
    { YSW_R2_C3, "B", YSW_MF_PLUS, on_chord_note, 71, chord_type_menu },
    { YSW_R2_C4, "C#", YSW_MF_PLUS, on_chord_note, 61, chord_type_menu },

    { YSW_R3_C1, "D#", YSW_MF_PLUS, on_chord_note, 63, chord_type_menu },
    { YSW_R3_C2, "F#", YSW_MF_PLUS, on_chord_note, 66, chord_type_menu },
    { YSW_R3_C3, "G#", YSW_MF_PLUS, on_chord_note, 68, chord_type_menu },
    { YSW_R3_C4, "A#", YSW_MF_PLUS, on_chord_note, 70, chord_type_menu },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C2, "Octave-", YSW_MF_PLUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C3, "Octave+", YSW_MF_PLUS, ysw_menu_nop, 0, NULL },

    { 0, "Chord", YSW_MF_END, NULL, 0, NULL },
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
    { YSW_R1_C1, "Note", YSW_MF_PLUS, ysw_menu_nop, 0, insert_note_duration_menu },
    { YSW_R1_C2, "Chord", YSW_MF_PLUS, ysw_menu_nop, 0, chord_octave4_menu },
    { YSW_R1_C3, "Beat", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R2_C1, "Rest", YSW_MF_PLUS, ysw_menu_nop, 0, rest_duration_menu },

    { YSW_R3_C1, "Tie", YSW_MF_COMMAND, on_insert_tie, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Insert", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t input_mode_menu[] = {
    { YSW_R1_C1, "Note", YSW_MF_COMMAND, on_melody_mode, 0, NULL },
    { YSW_R1_C2, "Chord", YSW_MF_COMMAND, on_chord_mode, 0, NULL },
    { YSW_R1_C3, "Rhythm", YSW_MF_COMMAND, on_rhythm_mode, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Input Mode", YSW_MF_END, NULL, 0, NULL },
};
const ysw_menu_item_t editor_menu[] = {
    { 0, "C#6", YSW_MF_BUTTON, on_note, 73, NULL },
    { 1, "D#6", YSW_MF_BUTTON, on_note, 75, NULL },
    { 2, "F#6", YSW_MF_BUTTON, on_note, 78, NULL },
    { 3, "G#6", YSW_MF_BUTTON, on_note, 80, NULL },
    { 4, "A#6", YSW_MF_BUTTON, on_note, 82, NULL },

    { 5, "Input\nMode", YSW_MF_PLUS, ysw_menu_nop, 0, input_mode_menu },
    { 6, "Insert", YSW_MF_PLUS, ysw_menu_nop, 0, insert_menu },
    { 8, "Up", YSW_MF_COMMAND, on_up, 0, NULL },

    { 9, "C6", YSW_MF_BUTTON, on_note, 72, NULL },
    { 10, "D6", YSW_MF_BUTTON, on_note, 74, NULL },
    { 11, "E6", YSW_MF_BUTTON, on_note, 76, NULL },
    { 12, "F6", YSW_MF_BUTTON, on_note, 77, NULL },
    { 13, "G6", YSW_MF_BUTTON, on_note, 79, NULL },
    { 14, "A6", YSW_MF_BUTTON, on_note, 81, NULL },
    { 15, "B6", YSW_MF_BUTTON, on_note, 83, NULL },

    { 16, "Listen", YSW_MF_PLUS, ysw_menu_nop, 0, listen_menu },
    { 17, "Edit", YSW_MF_PLUS, ysw_menu_nop, 0, edit_menu },
    { 19, "Down", YSW_MF_COMMAND, on_down, 0, NULL },

    { 20, "C#5", YSW_MF_BUTTON, on_note, 61, NULL },
    { 21, "D#5", YSW_MF_BUTTON, on_note, 63, NULL },
    { 22, "F#5", YSW_MF_BUTTON, on_note, 66, NULL },
    { 23, "G#5", YSW_MF_BUTTON, on_note, 68, NULL },
    { 24, "A#5", YSW_MF_BUTTON, on_note, 70, NULL },

    { 27, "Settings", YSW_MF_PLUS, ysw_menu_nop, 0, settings_menu },
    { 28, "Left", YSW_MF_COMMAND, on_left, 0, NULL },

    { 29, "C5", YSW_MF_BUTTON, on_note, 60, NULL },
    { 30, "D5", YSW_MF_BUTTON, on_note, 62, NULL },
    { 31, "E5", YSW_MF_BUTTON, on_note, 64, NULL },
    { 32, "F5", YSW_MF_BUTTON, on_note, 65, NULL },
    { 33, "G5", YSW_MF_BUTTON, on_note, 67, NULL },
    { 34, "A5", YSW_MF_BUTTON, on_note, 69, NULL },
    { 35, "B5", YSW_MF_BUTTON, on_note, 71, NULL },

    // TODO: Consider "save changes? yes/no/cancel" prompt to keep user from closing editor accidentally
    { 36, "Close\nMenu", YSW_MF_HIDE, ysw_menu_nop, 0, NULL },
    { 37, "Delete", YSW_MF_COMMAND, on_delete, 0, NULL },
    { 38, "Close\nEditor", YSW_MF_MINUS, on_close, 0, NULL },
    { 39, "Right", YSW_MF_COMMAND, on_right, 0, NULL },

    { 0, "Music", YSW_MF_END, NULL, 0, NULL },
};

void ysw_editor_edit_section(ysw_bus_t *bus, zm_music_t *music, zm_section_t *section)
{
    assert(music);
    assert(ysw_array_get_count(music->programs));
    assert(ysw_array_get_count(music->chord_types) > DEFAULT_CHORD_TYPE);
    assert(ysw_array_get_count(music->chord_styles));

    ysw_editor_t *editor = ysw_heap_allocate(sizeof(ysw_editor_t));

    editor->bus = bus;
    editor->music = music;
    editor->section = section;

    editor->chord_type = ysw_array_get(music->chord_types, DEFAULT_CHORD_TYPE);
    editor->chord_style = ysw_array_get(music->chord_styles, DEFAULT_CHORD_STYLE);
    editor->chord_frequency = ZM_ONE_PER_MEASURE;
    editor->beat = ysw_array_get(music->beats, DEFAULT_BEAT);
    editor->duration = ZM_AS_PLAYED;
    editor->drum_cadence = ZM_EIGHTH;

    editor->insert = true;
    editor->position = 0;
    editor->mode = YSW_EDITOR_MODE_MELODY;

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
    fire_program_change(editor, editor->section->melody_program, MELODY_CHANNEL);
    fire_program_change(editor, editor->section->chord_program, CHORD_CHANNEL);
    fire_program_change(editor, editor->section->rhythm_program, RHYTHM_CHANNEL);
    display_mode(editor); // mode displays program

    editor->menu = ysw_menu_create(bus, editor_menu, ysw_app_softkey_map, editor);
    ysw_menu_add_glass(editor->menu, editor->container);

    editor->queue = ysw_app_create_queue();
    ysw_bus_subscribe(bus, YSW_ORIGIN_KEYBOARD, editor->queue);
    ysw_bus_subscribe(bus, YSW_ORIGIN_SEQUENCER, editor->queue);

    ysw_app_handle_events(editor->queue, process_event, editor);

    ysw_bus_unsubscribe(bus, YSW_ORIGIN_KEYBOARD, editor->queue);
    ysw_bus_unsubscribe(bus, YSW_ORIGIN_SEQUENCER, editor->queue);
    ysw_bus_delete_queue(bus, editor->queue);

    ysw_menu_free(editor->menu);
    lv_obj_del(editor->container);
    ysw_heap_free(editor);
}

