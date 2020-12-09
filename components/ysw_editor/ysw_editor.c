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

#define DEFAULT_MELODY_PROGRAM 0
#define DEFAULT_CHORD_PROGRAM 1
#define DEFAULT_RHYTHM_PROGRAM 7

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
    ysw_bus_h bus;
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
} context_t;

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

static inline bool is_division_position(context_t *context)
{
    return context->position % 2 == 1;
}

static inline bool is_space_position(context_t *context)
{
    return context->position % 2 == 0;
}
 
static inline uint32_t division_index_to_position(zm_division_x division_index)
{
    return (division_index * 2) + 1;
}

// TODO: remove unnecessary calls to recalculate (e.g. increase/decrease pitch)

static void recalculate(context_t *context)
{
    zm_time_x start = 0;
    zm_measure_x measure = 1;
    uint32_t ticks_in_measure = 0;

    zm_division_x division_count = ysw_array_get_count(context->pattern->divisions);
    for (zm_division_x i = 0; i < division_count; i++) {
        zm_division_t *division = ysw_array_get(context->pattern->divisions, i);
        division->start = start;
        division->flags = 0;
        division->measure = measure;
        ticks_in_measure += zm_round_duration(division->melody.duration, NULL, NULL);
        if (ticks_in_measure >= 1024) {
            division->flags |= ZM_DIVISION_END_OF_MEASURE;
            ticks_in_measure = 0;
            measure++;
        }
        start += division->melody.duration;
    }

    ysw_staff_invalidate(context->staff);
}

static void play_division(context_t *context, zm_division_t *division)
{
    ysw_array_t *notes = zm_render_division(context->music, context->pattern, division, BASE_CHANNEL);
    zm_bpm_x bpm = zm_tempo_to_bpm(context->pattern->tempo);
    ysw_event_fire_play(context->bus, notes, bpm);
}

static void play_position(context_t *context)
{
    if (is_division_position(context)) {
        zm_division_x division_index = context->position / 2;
        zm_division_t *division = ysw_array_get(context->pattern->divisions, division_index);
        play_division(context, division);
    }
}

static void display_program(context_t *context)
{
    const char *value = "";
    if (context->mode == YSW_EDITOR_MODE_MELODY) {
        value = context->pattern->melody_program->name;
    } else if (context->mode == YSW_EDITOR_MODE_CHORD) {
        value = context->pattern->chord_program->name;
    } else if (context->mode == YSW_EDITOR_MODE_RHYTHM) {
        value = context->pattern->rhythm_program->name;
    }
    ysw_header_set_program(context->header, value);
}

static void display_melody_mode(context_t *context)
{
    // 1. on a division - show note or rest
    // 2. between divisions - show previous (to left) note or rest
    // 3. no divisions - show blank
    char value[32] = {};
    zm_division_x division_count = ysw_array_get_count(context->pattern->divisions);
    if (division_count) {
        zm_division_x division_index = context->position / 2;
        if (division_index >= division_count) {
            division_index = division_count - 1;
        }
        zm_bpm_x bpm = zm_tempo_to_bpm(context->pattern->tempo);
        zm_division_t *division = ysw_array_get(context->pattern->divisions, division_index);
        uint32_t millis = ysw_ticks_to_millis(division->melody.duration, bpm);
        zm_note_t note = division->melody.note;
        if (note) {
            uint8_t octave = (note / 12) - 1;
            const char *name = zm_get_note_name(note);
            snprintf(value, sizeof(value), "%s%d (%d ms)", name, octave, millis);
        } else {
            snprintf(value, sizeof(value), "Rest (%d ms)", millis);
        }
    }
    ysw_header_set_mode(context->header, modes[context->mode], value);
}

static void display_chord_mode(context_t *context)
{
    // 1. on a division with a chord value - show chord value, quality, style
    // 2. on a division without a chord value - show quality, style
    // 3. between divisions - show quality, style
    // 4. no divisions - show quality, style
    char value[32] = {};
    zm_division_t *division = NULL;
    if (is_division_position(context)) {
        division = ysw_array_get(context->pattern->divisions, context->position / 2);
    }
    if (division && division->chord.root) {
        snprintf(value, sizeof(value), "%s %s %s",
                zm_get_note_name(division->chord.root),
                division->chord.quality->name,
                division->chord.style->name);
    } else {
        snprintf(value, sizeof(value), "%s %s",
                context->quality->name,
                context->style->name);
    }
    ysw_header_set_mode(context->header, modes[context->mode], value);
}

static void display_rhythm_mode(context_t *context)
{
    char value[32] = {};
    zm_division_t *division = NULL;
    if (is_division_position(context)) {
        division = ysw_array_get(context->pattern->divisions, context->position / 2);
    }
    if (division && division->rhythm.beat) {
        snprintf(value, sizeof(value), "%s", division->rhythm.beat->name);
    } else {
        snprintf(value, sizeof(value), "%s", context->beat->name);
    }
    ysw_header_set_mode(context->header, modes[context->mode], value);
}

static void display_mode(context_t *context)
{
    switch (context->mode) {
        case YSW_EDITOR_MODE_MELODY:
            display_melody_mode(context);
            break;
        case YSW_EDITOR_MODE_CHORD:
            display_chord_mode(context);
            break;
        case YSW_EDITOR_MODE_RHYTHM:
            display_rhythm_mode(context);
            break;
    }
    // program is mode specific
    display_program(context);
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

static void fire_program_change(context_t *context, zm_program_t *program, zm_channel_x channel)
{
    ysw_event_program_change_t program_change = {
        .channel = channel,
        .program = ysw_array_find(context->music->programs, program),
    };
    ysw_event_fire_program_change(context->bus, YSW_ORIGIN_EDITOR, &program_change);
}

static void cycle_program(context_t *context)
{
    if (context->mode == YSW_EDITOR_MODE_MELODY) {
        context->pattern->melody_program = next_program(context->music->programs,
                context->pattern->melody_program, ZM_PROGRAM_NOTE);
        fire_program_change(context, context->pattern->melody_program, MELODY_CHANNEL);
    } else if (context->mode == YSW_EDITOR_MODE_CHORD) {
        context->pattern->chord_program = next_program(context->music->programs,
                context->pattern->chord_program, ZM_PROGRAM_NOTE);
        fire_program_change(context, context->pattern->chord_program, CHORD_CHANNEL);
    } else if (context->mode == YSW_EDITOR_MODE_RHYTHM) {
        context->pattern->rhythm_program = next_program(context->music->programs,
                context->pattern->rhythm_program, ZM_PROGRAM_BEAT);
        fire_program_change(context, context->pattern->rhythm_program, RHYTHM_CHANNEL);
    }
    display_program(context);
    play_position(context);
}

static void cycle_key_signature(context_t *context)
{
    context->pattern->key = zm_get_next_key_index(context->pattern->key);
    recalculate(context);
    ysw_footer_set_key(context->footer, context->pattern->key);
}

static void cycle_time_signature(context_t *context)
{
    context->pattern->time = zm_get_next_time_index(context->pattern->time);
    recalculate(context);
    ysw_footer_set_time(context->footer, context->pattern->time);
}

static void cycle_tempo(context_t *context)
{
    context->pattern->tempo = zm_get_next_tempo_index(context->pattern->tempo);
    recalculate(context);
    ysw_footer_set_tempo(context->footer, context->pattern->tempo);
    display_mode(context);
}

static void cycle_duration(context_t *context)
{
    zm_division_t *division = NULL;
    if (is_division_position(context) && context->duration != ZM_AS_PLAYED) {
        zm_division_x division_index = context->position / 2;
        division = ysw_array_get(context->pattern->divisions, division_index);
    }
    // Cycle default duration if not on a division or if division duration is already default duration
    if (!division || division->melody.duration == context->duration) {
        context->duration = zm_get_next_duration(context->duration);
        ysw_footer_set_duration(context->footer, context->duration);
    }
    if (division) {
        division->melody.duration = context->duration;
        recalculate(context);
        display_mode(context);
    }
}

static void cycle_tie(context_t *context)
{
    if (is_division_position(context)) {
        zm_division_x division_index = context->position / 2;
        zm_division_t *division = ysw_array_get(context->pattern->divisions, division_index);
        if (division->melody.note) {
            if (division->melody.tie) {
                division->melody.tie = 0;
            } else {
                division->melody.tie = 1;
            }
            ysw_staff_invalidate(context->staff);
        }
    }
}

static zm_style_t *find_matching_style(context_t *context, bool preincrement)
{

    zm_style_x current = ysw_array_find(context->music->styles, context->style);
    zm_style_x style_count = ysw_array_get_count(context->music->styles);
    zm_distance_x distance_count = ysw_array_get_count(context->quality->distances);
    for (zm_style_x i = 0; i < style_count; i++) {
        if (preincrement) {
            current = (current + 1) % style_count;
        }
        zm_style_t *style = ysw_array_get(context->music->styles, current);
        if (style->distance_count == distance_count) {
            context->style = style;
            return style;
        }
        if (!preincrement) {
            current = (current + 1) % style_count;
        }
    }
    assert(false);
}

static void cycle_quality(context_t *context)
{
    zm_division_t *division = NULL;
    if (is_division_position(context)) {
        zm_division_x division_index = context->position / 2;
        division = ysw_array_get(context->pattern->divisions, division_index);
    }
    if (!division || !division->chord.root || division->chord.quality == context->quality) {
        zm_quality_x previous = ysw_array_find(context->music->qualities, context->quality);
        zm_quality_x quality_count = ysw_array_get_count(context->music->qualities);
        zm_quality_x next = (previous + 1) % quality_count;
        context->quality = ysw_array_get(context->music->qualities, next);
        context->style = find_matching_style(context, false);
    }
    if (division) {
        division->chord.quality = context->quality;
        division->chord.style = context->style;
        ysw_staff_invalidate(context->staff);
    }
    display_mode(context);
    play_position(context);
}

static void cycle_style(context_t *context)
{
    zm_division_t *division = NULL;
    if (is_division_position(context)) {
        zm_division_x division_index = context->position / 2;
        division = ysw_array_get(context->pattern->divisions, division_index);
    }
    if (!division || !division->chord.root || division->chord.style == context->style) {
        context->style = find_matching_style(context, true);
    }
    if (division) {
        division->chord.style = context->style;
    }
    display_mode(context);
    play_position(context);
}

static void cycle_beat(context_t *context)
{
    zm_division_t *division = NULL;
    if (is_division_position(context)) {
        zm_division_x division_index = context->position / 2;
        division = ysw_array_get(context->pattern->divisions, division_index);
    }
    if (!division || !division->rhythm.beat || division->rhythm.beat == context->beat) {
        zm_beat_x previous = ysw_array_find(context->music->beats, context->beat);
        zm_beat_x beat_count = ysw_array_get_count(context->music->beats);
        zm_beat_x next = (previous + 1) % beat_count;
        context->beat = ysw_array_get(context->music->beats, next);
    }
    if (division) {
        division->rhythm.beat = context->beat;
        ysw_staff_invalidate(context->staff);
    }
    display_mode(context);
    play_position(context);
}

static zm_division_t *apply_note(context_t *context, zm_note_t midi_note, zm_time_x duration_millis)
{
    zm_time_x articulation = 0;
    zm_duration_t duration = context->duration;
    if (duration == ZM_AS_PLAYED) {
        zm_bpm_x bpm = zm_tempo_to_bpm(context->pattern->tempo);
        duration = ysw_millis_to_ticks(duration_millis, bpm);
        articulation = ysw_millis_to_ticks(context->delta, bpm);
    }

    zm_division_t *division = NULL;
    int32_t division_index = context->position / 2;
    bool is_new_division = is_space_position(context);
    if (is_new_division) {
        if (articulation && division_index) {
            zm_division_t *rest = ysw_heap_allocate(sizeof(zm_division_t));
            rest->melody.duration = min(articulation, 1024);
            ysw_array_insert(context->pattern->divisions, division_index, rest);
            division_index++;
            context->position = division_index * 2 + 1;
        }
        division = ysw_heap_allocate(sizeof(zm_division_t));
        ysw_array_insert(context->pattern->divisions, division_index, division);
    } else {
        division = ysw_array_get(context->pattern->divisions, division_index);
    }

    if (context->mode == YSW_EDITOR_MODE_MELODY) {
        division->melody.note = midi_note; // rest == 0
        division->melody.duration = duration;
    } else if (context->mode == YSW_EDITOR_MODE_CHORD) {
        division->chord.root = midi_note; // rest == 0
        division->chord.quality = context->quality;
        division->chord.style = context->style;
        division->chord.duration = duration;
        if (!division->melody.duration) {
            division->melody.duration = division->chord.duration;
        }
    } else if (context->mode == YSW_EDITOR_MODE_RHYTHM) {
        division->rhythm.surface = midi_note; // rest == 0
        if (!midi_note) {
            division->rhythm.beat = NULL;
        }
        if (!division->melody.duration) {
            division->melody.duration = duration;
        }
    }

    zm_division_x division_count = ysw_array_get_count(context->pattern->divisions);
    context->position = min(context->position + context->advance, division_count * 2);
    ysw_staff_set_position(context->staff, context->position);
    display_mode(context);
    recalculate(context);
    return division;
}

static void insert_beat(context_t *context)
{
    uint32_t last_start = 0;
    assert(context->beat);
    if (!is_space_position(context)) {
        context->position++;
    }
    zm_duration_t saved_duration = context->duration;
    zm_beat_x stroke_count = ysw_array_get_count(context->beat->strokes);
    for (zm_beat_x i = 0; i <= stroke_count; i++) {
        zm_time_x start;
        if (i < stroke_count) {
            zm_stroke_t *stroke = ysw_array_get(context->beat->strokes, i);
            start = stroke->start;
        } else {
            start = 1024;
        }
        if (start != last_start) {
            context->duration = zm_round_duration(start - last_start, NULL, NULL);
            zm_division_t *division = apply_note(context, 0, 0);
            if (!last_start) {
                division->rhythm.beat = context->beat;
            }
            last_start = start;
        }
    }
    context->duration = saved_duration;
}

static void process_delete(context_t *context)
{
    zm_division_x division_index = context->position / 2;
    zm_division_x division_count = ysw_array_get_count(context->pattern->divisions);
    if (division_count > 0 && division_index == division_count) {
        // On space following last division in pattern, delete previous division
        division_index--;
    }
    if (division_index < division_count) {
        zm_division_t *division = ysw_array_remove(context->pattern->divisions, division_index);
        ysw_heap_free(division);
        division_count--;
        if (division_index == division_count) {
            if (division_count) {
                context->position -= 2;
            } else {
                context->position = 0;
            }
        }
        ysw_staff_set_position(context->staff, context->position);
        recalculate(context);
    }
}

static void process_left(context_t *context, uint8_t move_amount)
{
    ESP_LOGD(TAG, "process_left");
    if (context->position >= move_amount) {
        context->position -= move_amount;
        play_position(context);
        ysw_staff_set_position(context->staff, context->position);
        display_mode(context);
    }
}

static void process_right(context_t *context, uint8_t move_amount)
{
    uint32_t new_position = context->position + move_amount;
    if (new_position <= ysw_array_get_count(context->pattern->divisions) * 2) {
        context->position = new_position;
        play_position(context);
        ysw_staff_set_position(context->staff, context->position);
        display_mode(context);
    }
}

static void process_up(context_t *context)
{
    if (is_division_position(context)) {
        zm_division_x division_index = context->position / 2;
        zm_division_t *division = ysw_array_get(context->pattern->divisions, division_index);
        if (context->mode == YSW_EDITOR_MODE_MELODY) {
            if (division->melody.note && division->melody.note < 83) {
                division->melody.note++;
            }
        } else if (context->mode == YSW_EDITOR_MODE_CHORD) {
            if (division->chord.root && division->chord.root < 83) {
                division->chord.root++;
            }
        } else if (context->mode == YSW_EDITOR_MODE_RHYTHM) {
        }
        play_position(context);
        recalculate(context);
        display_mode(context);
    }
}

static void process_down(context_t *context)
{
    if (is_division_position(context)) {
        zm_division_x division_index = context->position / 2;
        zm_division_t *division = ysw_array_get(context->pattern->divisions, division_index);
        if (context->mode == YSW_EDITOR_MODE_MELODY) {
            if (division->melody.note && division->melody.note > 60) {
                division->melody.note--;
            }
        } else if (context->mode == YSW_EDITOR_MODE_CHORD) {
            if (division->chord.root && division->chord.root > 60) {
                division->chord.root--;
            }
        } else if (context->mode == YSW_EDITOR_MODE_RHYTHM) {
        }
        play_position(context);
        recalculate(context);
        display_mode(context);
    }
}

static void on_melody(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    context->mode = YSW_EDITOR_MODE_MELODY;
    display_mode(context);
}

static void on_chord(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    context->mode = YSW_EDITOR_MODE_CHORD;
    display_mode(context);
}

static void on_rhythm(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    context->mode = YSW_EDITOR_MODE_RHYTHM;
    display_mode(context);
}

static void on_program(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    cycle_program(context);
}

static void on_quality(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    cycle_quality(context);
}

static void on_style(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    cycle_style(context);
}

static void on_key_signature(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    cycle_key_signature(context);
}

static void on_time_signature(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    cycle_time_signature(context);
}

static void on_tempo(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    cycle_tempo(context);
}

static void on_duration(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    cycle_duration(context);
}

static void on_tie(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    cycle_tie(context);
}

static void on_cycle_beat(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    cycle_beat(context);
}

static void on_insert_beat(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    insert_beat(context);
}

static void on_delete(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    process_delete(context);
}

static void on_play(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    ysw_array_t *notes = zm_render_pattern(context->music, context->pattern, BACKGROUND_BASE);
    zm_bpm_x bpm = zm_tempo_to_bpm(context->pattern->tempo);
    ysw_event_fire_play(context->bus, notes, bpm);
}

static void on_demo(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    zm_song_t *song = ysw_array_get(context->music->songs, 0);
    ysw_array_t *notes = zm_render_song(song);
    ysw_event_fire_play(context->bus, notes, song->bpm);
}

static void on_stop(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    ysw_event_fire_stop(context->bus);
}

static void on_loop(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    context->loop = !context->loop;
    ysw_event_fire_loop(context->bus, context->loop);
}

static void on_left(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    ESP_LOGD(TAG, "on_left");
    context_t *context = menu->caller_context;
    process_left(context, 1);
}

static void on_right(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    process_right(context, 1);
}

static void on_previous(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    process_left(context, 2);
}

static void on_next(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    process_right(context, 2);
}

static void on_up(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    process_up(context);
}

static void on_down(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    process_down(context);
}

static void fire_note_on(context_t *context, zm_note_t midi_note, zm_channel_x channel)
{
    ysw_event_note_on_t note_on = {
        .channel = channel,
        .midi_note = midi_note,
        .velocity = 80,
    };
    ysw_event_fire_note_on(context->bus, YSW_ORIGIN_EDITOR, &note_on);
}

static void fire_note_off(context_t *context, zm_note_t midi_note, zm_channel_x channel)
{
    ysw_event_note_off_t note_off = {
        .channel = channel,
        .midi_note = midi_note,
    };
    ysw_event_fire_note_off(context->bus, YSW_ORIGIN_EDITOR, &note_off);
}

static void on_note(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    zm_note_t midi_note = (uintptr_t)value;
    if (event->header.type == YSW_EVENT_KEY_DOWN) {
        if (context->mode == YSW_EDITOR_MODE_MELODY) {
            context->down_at = event->key_down.time;
            context->delta = context->down_at - context->up_at;
            if (midi_note) {
                fire_note_on(context, midi_note, MELODY_CHANNEL);
            }
        } else if (context->mode == YSW_EDITOR_MODE_CHORD) {
            zm_division_t division = {
                .chord.root = midi_note,
                .chord.quality = context->quality,
                .chord.style = context->style,
                .chord.duration = 512,
            };
            play_division(context, &division);
        } else if (context->mode == YSW_EDITOR_MODE_RHYTHM) {
            if (midi_note) {
                fire_note_on(context, midi_note, RHYTHM_CHANNEL);
            }
        }
    } else if (event->header.type == YSW_EVENT_KEY_UP) {
        if (context->mode == YSW_EDITOR_MODE_MELODY) {
            context->up_at = event->key_up.time;
            if (midi_note) {
                fire_note_off(context, midi_note, MELODY_CHANNEL);
            }
        } else if (context->mode == YSW_EDITOR_MODE_RHYTHM) {
            if (midi_note) {
                fire_note_off(context, midi_note, RHYTHM_CHANNEL);
            }
        }
        apply_note(context, midi_note, event->key_pressed.duration);
    }
}

int compare_divisions(const void *left, const void *right)
{
    const zm_division_t *left_division = *(zm_division_t * const *)left;
    const zm_division_t *right_division = *(zm_division_t * const *)right;
    int delta = left_division->start - right_division->start;
    return delta;
}

static void on_note_status(context_t *context, ysw_event_t *event)
{
    if (event->note_status.note.channel >= BACKGROUND_BASE) {
        zm_division_t needle = {
            .start = event->note_status.note.start,
        };
        ysw_array_match_t flags = YSW_ARRAY_MATCH_EXACT;
        int32_t result = ysw_array_search(context->pattern->divisions, &needle, compare_divisions, flags);
        if (result != -1) {
            context->position = division_index_to_position(result);
            ysw_staff_set_position(context->staff, context->position);
        } else {
            // e.g. a stroke in a beat
        }
    }
}

static void process_event(void *caller_context, ysw_event_t *event)
{
    context_t *context = caller_context;
    if (event) {
        switch (event->header.type) {
            case YSW_EVENT_KEY_DOWN:
                ysw_menu_on_key_down(context->menu, event);
                break;
            case YSW_EVENT_KEY_UP:
                ysw_menu_on_key_up(context->menu, event);
                break;
            case YSW_EVENT_KEY_PRESSED:
                ysw_menu_on_key_pressed(context->menu, event);
                break;
            case YSW_EVENT_NOTE_STATUS:
                on_note_status(context, event);
                break;
            default:
                break;
        }
    }
    lv_task_handler();
}

static void initialize_editor_task(void *caller_context)
{
    context_t *context = caller_context;

    context->lvgl_init();
    context->container = lv_obj_create(lv_scr_act(), NULL);
    assert(context->container);

    // See https://docs.lvgl.io/v7/en/html/overview/style.html
    lv_obj_set_style_local_bg_color(context->container, 0, 0, LV_COLOR_MAROON);
    lv_obj_set_style_local_bg_grad_color(context->container, 0, 0, LV_COLOR_BLACK);
    lv_obj_set_style_local_bg_grad_dir(context->container, 0, 0, LV_GRAD_DIR_VER);
    lv_obj_set_style_local_bg_opa(context->container, 0, 0, LV_OPA_100);

    lv_obj_set_style_local_text_color(context->container, 0, 0, LV_COLOR_YELLOW);
    lv_obj_set_style_local_text_opa(context->container, 0, 0, LV_OPA_100);

    lv_obj_set_size(context->container, 320, 240);
    lv_obj_align(context->container, NULL, LV_ALIGN_CENTER, 0, 0);

    context->header = ysw_header_create(context->container);
    lv_obj_set_size(context->header, 320, 30);
    lv_obj_align(context->header, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);
    display_mode(context);
    display_program(context);

    context->staff = ysw_staff_create(context->container);
    assert(context->staff);
    lv_obj_set_size(context->staff, 320, 180);
    lv_obj_align(context->staff, NULL, LV_ALIGN_CENTER, 0, 0);
    ysw_staff_set_pattern(context->staff, context->pattern);

    context->footer = ysw_footer_create(context->container);
    lv_obj_set_size(context->footer, 320, 30);
    lv_obj_align(context->footer, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    ysw_footer_set_key(context->footer, context->pattern->key);
    ysw_footer_set_time(context->footer, context->pattern->time);
    ysw_footer_set_tempo(context->footer, context->pattern->tempo);
    ysw_footer_set_duration(context->footer, context->duration);
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
#define YSW_MF_PLUS (YSW_MENU_DOWN|YSW_MENU_OPEN)
#define YSW_MF_MINUS (YSW_MENU_UP|YSW_MENU_POP|YSW_MF_BUTTON)
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

    { 25, "Note\nLength+", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 26, "Note\nLength-", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 27, "Clear\nNote", YSW_MF_COMMAND, ysw_menu_nop, 0 },

    { 36, " ", YSW_MF_PLUS, ysw_menu_nop, 0 },

    { 40, NULL, 0, NULL, NULL },
};

static const ysw_menu_item_t chord_menu[] = {
    { 5, "Chord\nQuality", YSW_MF_COMMAND, on_quality, 0 },
    { 6, "Chord\nStyle", YSW_MF_COMMAND, on_style, 0 },
    { 7, "Chord\nSound", YSW_MF_COMMAND, on_program, 0 },

    { 16, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 17, " ", YSW_MF_NOP, ysw_menu_nop, 0 },
    { 18, " ", YSW_MF_NOP, ysw_menu_nop, 0 },

    { 25, "Chord\nLength+", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 26, "Chord\nLength-", YSW_MF_COMMAND, ysw_menu_nop, 0 },
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
    { 5, "New", YSW_MF_COMMAND, ysw_menu_nop, 0 },
    { 6, "Save", YSW_MF_COMMAND, ysw_menu_nop, 0 },
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

void ysw_editor_create_task(ysw_bus_h bus, zm_music_t *music, ysw_editor_lvgl_init lvgl_init)
{
    assert(bus);
    assert(music);
    assert(lvgl_init);

    assert(ysw_array_get_count(music->programs));
    assert(ysw_array_get_count(music->qualities) > DEFAULT_QUALITY);
    assert(ysw_array_get_count(music->styles));

    context_t *context = ysw_heap_allocate(sizeof(context_t));

    context->bus = bus;
    context->lvgl_init = lvgl_init;
    context->music = music;

    if (ysw_array_get_count(music->patterns) > 0) {
        context->pattern = ysw_array_get(music->patterns, 0);
    } else {
        context->pattern = ysw_heap_allocate(sizeof(zm_pattern_t));
        context->pattern->divisions = ysw_array_create(64);
        context->pattern->key = ZM_KEY_C;
        context->pattern->time = ZM_TIME_4_4;
        context->pattern->tempo = ZM_TEMPO_100;
        context->pattern->melody_program = ysw_array_get(music->programs, DEFAULT_MELODY_PROGRAM);
        context->pattern->chord_program = ysw_array_get(music->programs, DEFAULT_CHORD_PROGRAM);
        context->pattern->rhythm_program = ysw_array_get(music->programs, DEFAULT_RHYTHM_PROGRAM);
    }

    context->quality = ysw_array_get(music->qualities, DEFAULT_QUALITY);
    context->style = ysw_array_get(music->styles, DEFAULT_STYLE);
    context->beat = ysw_array_get(music->beats, DEFAULT_BEAT);
    context->duration = ZM_AS_PLAYED;

    context->advance = 2;
    context->position = 0;
    context->mode = YSW_EDITOR_MODE_MELODY;

    fire_program_change(context, context->pattern->melody_program, MELODY_CHANNEL);
    fire_program_change(context, context->pattern->chord_program, CHORD_CHANNEL);
    fire_program_change(context, context->pattern->rhythm_program, RHYTHM_CHANNEL);

    context->menu = ysw_menu_create(main_menu, softmap, context);

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.initializer = initialize_editor_task;
    config.event_handler = process_event;
    config.caller_context = context;
    config.wait_millis = 5;
    //config.priority = YSW_TASK_DEFAULT_PRIORITY + 1;
    //config.priority = configMAX_PRIORITIES - 1;
    config.priority = 24;

    ysw_task_h task = ysw_task_create(&config);

    ysw_task_subscribe(task, YSW_ORIGIN_KEYBOARD);
    ysw_task_subscribe(task, YSW_ORIGIN_SEQUENCER);
}

