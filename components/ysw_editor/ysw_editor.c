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

// Note that position is 2x the beat index and encodes both the beat index and the space before it.

// 0 % 2 = 0
// 1 % 2 = 1 C
// 2 % 2 = 0
// 3 % 2 = 1 E
// 4 % 2 = 0
// 5 % 2 = 1 G
// 6 % 2 = 0

// A remainder of zero indicates the space and a remainder of 1 indicates
// the beat. This happens to work at the end of the array too, if the position
// is 2x the size of the array, it refers to the space following the last beat.

// if (position / 2 >= size) {
//   -> space after last beat
// } else {
//   if (position % 2 == 0) {
//     -> space before beat
//   } else {
//     -> beat_index = position / 2;
//   }
// }

typedef enum {
    YSW_EDITOR_MODE_TONE,
    YSW_EDITOR_MODE_CHORD,
    YSW_EDITOR_MODE_DRUM,
} ysw_editor_mode_t;

static const char *modes[] = {
    "Note",
    "Chord",
    "Drum",
};

#define YSW_EDITOR_MODE_COUNT 3

typedef struct {
    ysw_bus_h bus;
    zm_music_t *music;
    ysw_editor_lvgl_init lvgl_init;
    zm_passage_t *passage;
    zm_sample_t *tone_sample;
    zm_sample_t *chord_sample;
    zm_quality_t *quality;
    zm_style_t *style;
    zm_duration_t duration;

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

static void play_beat(context_t *context, zm_beat_t *beat)
{
    ysw_array_t *notes = ysw_array_create(16);
    if (beat->tone.note) {
        zm_sample_x sample_index = ysw_array_find(context->music->samples, context->tone_sample);
        zm_render_tone(notes, &beat->tone, 0, 1, sample_index);
    }
    if (beat->chord.root) {
        zm_sample_x sample_index = ysw_array_find(context->music->samples, context->chord_sample);
        zm_render_step(notes, &beat->chord, 0, 2, sample_index);
    }
    ysw_array_sort(notes, zm_note_compare);
    zm_bpm_x bpm = zm_tempo_to_bpm(context->passage->tempo);
    ysw_event_fire_play(context->bus, notes, bpm); // TODO: consider supplying origin or use channel
}

static void play_position(context_t *context)
{
    if (context->position % 2 == 1) {
        zm_beat_x beat_index = context->position / 2;
        zm_beat_t *beat = ysw_array_get(context->passage->beats, beat_index);
        play_beat(context, beat);
    }
}

static void display_sample(context_t *context)
{
    const char *value = "";
    if (context->mode == YSW_EDITOR_MODE_TONE) {
        value = context->tone_sample->name;
    } else if (context->mode == YSW_EDITOR_MODE_CHORD) {
        value = context->chord_sample->name;
    }
    ysw_header_set_sample(context->header, value);
}

static void display_tone_mode(context_t *context)
{
    // 1. on a beat - show note or rest
    // 2. between beats - show previous (to left) note or rest
    // 3. no beats - show blank
    char value[32] = {};
    zm_beat_x beat_count = ysw_array_get_count(context->passage->beats);
    if (beat_count) {
        zm_beat_x beat_index = context->position / 2;
        if (beat_index >= beat_count) {
            beat_index = beat_count - 1;
        }
        zm_bpm_x bpm = zm_tempo_to_bpm(context->passage->tempo);
        zm_beat_t *beat = ysw_array_get(context->passage->beats, beat_index);
        uint32_t millis = ysw_ticks_to_millis(beat->tone.duration, bpm);
        zm_note_t note = beat->tone.note;
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
    // 1. on a beat with a chord value - show chord value, quality, style
    // 2. on a beat without a chord value - show quality, style
    // 3. between beats - show quality, style
    // 4. no beats - show quality, style
    char value[32] = {};
    zm_beat_t *beat = NULL;
    if (context->position % 2 == 1) {
        beat = ysw_array_get(context->passage->beats, context->position / 2);
    }
    if (beat && beat->chord.root) {
        snprintf(value, sizeof(value), "%s %s %s",
                zm_get_note_name(beat->chord.root),
                beat->chord.quality->name,
                beat->chord.style->name);
    } else {
        snprintf(value, sizeof(value), "%s %s",
                context->quality->name,
                context->style->name);
    }
    ysw_header_set_mode(context->header, modes[context->mode], value);
}

static void display_drum_mode(context_t *context)
{
    ysw_header_set_mode(context->header, modes[context->mode], "");
}

static void display_mode(context_t *context)
{
    switch (context->mode) {
        case YSW_EDITOR_MODE_TONE:
            display_tone_mode(context);
            break;
        case YSW_EDITOR_MODE_CHORD:
            display_chord_mode(context);
            break;
        case YSW_EDITOR_MODE_DRUM:
            display_drum_mode(context);
            break;
    }
    // sample is mode specific
    display_sample(context);
}

static zm_sample_t *next_sample(ysw_array_t *samples, zm_sample_t *previous)
{
    zm_sample_x sample_count = ysw_array_get_count(samples);
    zm_sample_x sample_index = ysw_array_find(samples, previous);
    sample_index = (sample_index + 1) % sample_count;
    zm_sample_t *next = ysw_array_get(samples, sample_index);
    return next;
}

static void cycle_sample(context_t *context)
{
    if (context->mode == YSW_EDITOR_MODE_TONE) {
        context->tone_sample = next_sample(context->music->samples, context->tone_sample);
        ysw_event_program_change_t program_change = {
            .channel = 0,
            .program = ysw_array_find(context->music->samples, context->tone_sample),
        };
        ysw_event_fire_program_change(context->bus, YSW_ORIGIN_EDITOR, &program_change);
    } else if (context->mode == YSW_EDITOR_MODE_CHORD) {
        context->chord_sample = next_sample(context->music->samples, context->chord_sample);
    }
    display_sample(context);
}

static void cycle_key_signature(context_t *context)
{
    context->passage->key = zm_get_next_key_index(context->passage->key);
    ysw_staff_update_all(context->staff, context->position);
    ysw_footer_set_key(context->footer, context->passage->key);
}

static void cycle_time_signature(context_t *context)
{
    context->passage->time = zm_get_next_time_index(context->passage->time);
    ysw_staff_update_all(context->staff, context->position);
    ysw_footer_set_time(context->footer, context->passage->time);
}

static void cycle_tempo(context_t *context)
{
    context->passage->tempo = zm_get_next_tempo_index(context->passage->tempo);
    ysw_staff_update_all(context->staff, context->position);
    ysw_footer_set_tempo(context->footer, context->passage->tempo);
    display_mode(context);
}

static void cycle_duration(context_t *context)
{
    zm_beat_t *beat = NULL;
    if (context->position % 2 == 1 && context->duration != ZM_AS_PLAYED) {
        zm_beat_x beat_index = context->position / 2;
        beat = ysw_array_get(context->passage->beats, beat_index);
    }
    // Cycle default duration if not on a beat or if beat duration is already default duration
    if (!beat || beat->tone.duration == context->duration) {
        if (context->duration == ZM_AS_PLAYED) {
            context->duration = ZM_SIXTEENTH;
        } else if (context->duration <= ZM_SIXTEENTH) {
            context->duration = ZM_EIGHTH;
        } else if (context->duration <= ZM_EIGHTH) {
            context->duration = ZM_QUARTER;
        } else if (context->duration <= ZM_QUARTER) {
            context->duration = ZM_HALF;
        } else if (context->duration <= ZM_HALF) {
            context->duration = ZM_WHOLE;
        } else {
            context->duration = ZM_AS_PLAYED;
        }
        ysw_footer_set_duration(context->footer, context->duration);
    }
    if (beat) {
        beat->tone.duration = context->duration;
        ysw_staff_update_all(context->staff, context->position);
        display_mode(context);
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
    zm_beat_t *beat = NULL;
    if (context->position % 2 == 1) {
        zm_beat_x beat_index = context->position / 2;
        beat = ysw_array_get(context->passage->beats, beat_index);
    }
    if (!beat || !beat->chord.root || beat->chord.quality == context->quality) {
        zm_quality_x previous = ysw_array_find(context->music->qualities, context->quality);
        zm_quality_x quality_count = ysw_array_get_count(context->music->qualities);
        zm_quality_x next = (previous + 1) % quality_count;
        context->quality = ysw_array_get(context->music->qualities, next);
        context->style = find_matching_style(context, false);
    }
    if (beat) {
        beat->chord.quality = context->quality;
        beat->chord.style = context->style;
        ysw_staff_update_all(context->staff, context->position);
    }
    display_mode(context);
}

static void cycle_style(context_t *context)
{
    zm_beat_t *beat = NULL;
    if (context->position % 2 == 1) {
        zm_beat_x beat_index = context->position / 2;
        beat = ysw_array_get(context->passage->beats, beat_index);
    }
    if (!beat || !beat->chord.root || beat->chord.style == context->style) {
        context->style = find_matching_style(context, true);
    }
    if (beat) {
        beat->chord.style = context->style;
        ysw_staff_update_all(context->staff, context->position);
    }
    display_mode(context);
}

static void process_beat(context_t *context, zm_note_t midi_note, zm_time_x duration)
{
    zm_beat_t *beat = NULL;
    int32_t beat_index = context->position / 2;
    bool is_new_beat = context->position % 2 == 0;
    if (is_new_beat) {
        beat = ysw_heap_allocate(sizeof(zm_beat_t));
        ysw_array_insert(context->passage->beats, beat_index, beat);
    } else {
        beat = ysw_array_get(context->passage->beats, beat_index);
    }

    if (context->mode == YSW_EDITOR_MODE_TONE) {
        beat->tone.note = midi_note; // rest == 0
        if (context->duration == ZM_AS_PLAYED) {
            zm_bpm_x bpm = zm_tempo_to_bpm(context->passage->tempo);
            beat->tone.duration = ysw_millis_to_ticks(duration, bpm);
        } else {
            beat->tone.duration = context->duration;
        }
    } else if (context ->mode == YSW_EDITOR_MODE_CHORD) {
        if (is_new_beat) {
            beat->tone.duration = ZM_QUARTER;
        }
        beat->chord.root = midi_note;
        beat->chord.quality = context->quality;
        beat->chord.style = context->style;
        beat->chord.duration = ZM_HALF;
        play_beat(context, beat);
    } else if (context -> mode == YSW_EDITOR_MODE_DRUM) {
    }

    zm_beat_x beat_count = ysw_array_get_count(context->passage->beats);
    context->position = min(context->position + context->advance, beat_count * 2);
    ysw_staff_update_all(context->staff, context->position);
    display_mode(context);
}

static void process_delete(context_t *context)
{
    if (context->position % 2 == 1) {
        zm_beat_x beat_index = context->position / 2;
        zm_beat_t *beat = ysw_array_remove(context->passage->beats, beat_index);
        ysw_heap_free(beat);
        zm_beat_x beat_count = ysw_array_get_count(context->passage->beats);
        if (beat_index == beat_count) {
            if (beat_count) {
                context->position -= 2;
            } else {
                context->position = 0;
            }
        }
        ysw_staff_update_all(context->staff, context->position);
    }
}

static void process_left(context_t *context, uint8_t move_amount)
{
    ESP_LOGD(TAG, "process_left");
    if (context->position >= move_amount) {
        context->position -= move_amount;
        play_position(context);
        ysw_staff_update_position(context->staff, context->position);
        display_mode(context);
    }
}

static void process_right(context_t *context, uint8_t move_amount)
{
    uint32_t new_position = context->position + move_amount;
    if (new_position <= ysw_array_get_count(context->passage->beats) * 2) {
        context->position = new_position;
        play_position(context);
        ysw_staff_update_position(context->staff, context->position);
        display_mode(context);
    }
}

static void process_up(context_t *context)
{
    if (context->position % 2 == 1) {
        zm_beat_x beat_index = context->position / 2;
        zm_beat_t *beat = ysw_array_get(context->passage->beats, beat_index);
        if (context->mode == YSW_EDITOR_MODE_TONE) {
            if (beat->tone.note && beat->tone.note < 83) {
                beat->tone.note++;
            }
        } else if (context->mode == YSW_EDITOR_MODE_CHORD) {
            if (beat->chord.root && beat->chord.root < 83) {
                beat->chord.root++;
            }
        } else if (context->mode == YSW_EDITOR_MODE_DRUM) {
        }
        play_position(context);
        ysw_staff_update_all(context->staff, context->position);
        display_mode(context);
    }
}

static void process_down(context_t *context)
{
    if (context->position % 2 == 1) {
        zm_beat_x beat_index = context->position / 2;
        zm_beat_t *beat = ysw_array_get(context->passage->beats, beat_index);
        if (context->mode == YSW_EDITOR_MODE_TONE) {
            if (beat->tone.note && beat->tone.note > 60) {
                beat->tone.note--;
            }
        } else if (context->mode == YSW_EDITOR_MODE_CHORD) {
            if (beat->chord.root && beat->chord.root > 60) {
                beat->chord.root--;
            }
        } else if (context->mode == YSW_EDITOR_MODE_DRUM) {
        }
        play_position(context);
        ysw_staff_update_all(context->staff, context->position);
        display_mode(context);
    }
}

UNUSED
static void on_mode_tone(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    context->mode = YSW_EDITOR_MODE_TONE;
    display_mode(context);
}

UNUSED
static void on_mode_chord(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    context->mode = YSW_EDITOR_MODE_CHORD;
    display_mode(context);
}

UNUSED
static void on_mode_drum(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    context->mode = YSW_EDITOR_MODE_DRUM;
    display_mode(context);
}

static void on_cycle_mode(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    context->mode = (context->mode + 1) % 3;
    display_mode(context);
}

static void on_sample(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    cycle_sample(context);
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

static void on_delete(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    process_delete(context);
}

static void on_play(ysw_menu_t *menu, ysw_event_t *event, void *value)
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

static void on_note(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    context_t *context = menu->caller_context;
    zm_note_t midi_note = (uintptr_t)value;
    if (event->header.type == YSW_EVENT_KEY_DOWN) {
        if (context->mode == YSW_EDITOR_MODE_TONE) {
            if (midi_note) {
                ysw_event_note_on_t note_on = {
                    .channel = 0,
                    .midi_note = midi_note,
                    .velocity = 80,
                };
                ysw_event_fire_note_on(context->bus, YSW_ORIGIN_EDITOR, &note_on);
            }
        }
    } else if (event->header.type == YSW_EVENT_KEY_UP) {
        if (context->mode == YSW_EDITOR_MODE_TONE) {
            if (midi_note) {
                ysw_event_note_off_t note_off = {
                    .channel = 0,
                    .midi_note = midi_note,
                };
                ysw_event_fire_note_off(context->bus, YSW_ORIGIN_EDITOR, &note_off);
            }
        }
        process_beat(context, midi_note, event->key_pressed.duration);
    }
}

static void on_note_on(context_t *context, ysw_event_t *event)
{
    assert(event->header.origin == YSW_ORIGIN_SEQUENCER); // i.e. not from us
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
            case YSW_EVENT_NOTE_ON:
                on_note_on(context, event);
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
    display_sample(context);

    context->staff = ysw_staff_create(context->container);
    assert(context->staff);
    lv_obj_set_size(context->staff, 320, 180);
    lv_obj_align(context->staff, NULL, LV_ALIGN_CENTER, 0, 0);
    ysw_staff_set_passage(context->staff, context->passage);

    context->footer = ysw_footer_create(context->container);
    lv_obj_set_size(context->footer, 320, 30);
    lv_obj_align(context->footer, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    ysw_footer_set_key(context->footer, context->passage->key);
    ysw_footer_set_time(context->footer, context->passage->time);
    ysw_footer_set_tempo(context->footer, context->passage->tempo);
    ysw_footer_set_duration(context->footer, context->duration);
}

// Layout of keycodes generated by keyboard:
//
//    0,  1,      2,  3,  4,      5,  6,  7,  8,
//  9, 10, 11, 12, 13, 14, 15,   16, 17, 18, 19,
//   20, 21,     22, 23, 24,     25, 26, 27, 28,
// 29, 30, 31, 32, 33, 34, 35,   36, 37, 38, 39,

#define VP (void *)(uintptr_t)

static const ysw_menu_item_t menu_2[] = {
    /* 00 */ { "C#6", 0x03, on_note, VP 73 },
    /* 01 */ { "D#6", 0x03, on_note, VP 75 },
    /* 02 */ { "F#6", 0x03, on_note, VP 78 },
    /* 03 */ { "G#6", 0x03, on_note, VP 80 },
    /* 04 */ { "A#6", 0x03, on_note, VP 82 },
    /* 05 */ { "Play", 0x14, on_play, 0 },
    /* 06 */ { "Stop", 0x14, on_stop, 0 },
    /* 07 */ { "Loop", 0x14, on_loop, 0 },
    /* 08 */ { "Up", 0x34, on_up, 0 },
    /* 09 */ { "C6", 0x03, on_note, VP 72 },
    /* 10 */ { "D6", 0x03, on_note, VP 74 },
    /* 11 */ { "E6", 0x03, on_note, VP 76 },
    /* 12 */ { "F6", 0x03, on_note, VP 77 },
    /* 13 */ { "G6", 0x03, on_note, VP 79 },
    /* 14 */ { "A6", 0x03, on_note, VP 81 },
    /* 15 */ { "B6", 0x03, on_note, VP 83 },
    /* 16 */ { " ", 0x14, ysw_menu_nop, 0 },
    /* 17 */ { " ", 0x14, ysw_menu_nop, 0 },
    /* 18 */ { " ", 0x14, ysw_menu_nop, 0 },
    /* 19 */ { "Down", 0x34, on_down, 0 },
    /* 20 */ { "C#5", 0x03, on_note, VP 61 },
    /* 21 */ { "D#5", 0x03, on_note, VP 63 },
    /* 22 */ { "F#5", 0x03, on_note, VP 66 },
    /* 23 */ { "G#5", 0x03, on_note, VP 68 },
    /* 24 */ { "A#5", 0x03, on_note, VP 70 },
    /* 25 */ { " ", 0x14, ysw_menu_nop, 0 },
    /* 26 */ { " ", 0x14, ysw_menu_nop, 0 },
    /* 27 */ { " ", 0x14, ysw_menu_nop, 0 },
    /* 28 */ { "Previous", 0x34, on_previous, 0 },
    /* 29 */ { "C5", 0x03, on_note, VP 60 },
    /* 30 */ { "D5", 0x03, on_note, VP 62 },
    /* 31 */ { "E5", 0x03, on_note, VP 64 },
    /* 32 */ { "F5", 0x03, on_note, VP 65 },
    /* 33 */ { "G5", 0x03, on_note, VP 67 },
    /* 34 */ { "A5", 0x03, on_note, VP 69 },
    /* 35 */ { "B5", 0x03, on_note, VP 71 },
    /* 36 */ { "Menu+", 0x13, ysw_menu_on_open, 0 },
    /* 37 */ { " ", 0x14, ysw_menu_nop, 0 },
    /* 38 */ { "Menu-", 0x13, ysw_menu_on_close, 0 },
    /* 39 */ { "Next", 0x14, on_next, 0 },
    /* 40 */ { NULL, 0, NULL, NULL },
};

static const ysw_menu_item_t base_menu[] = {
    /* 00 */ { "C#6", 0x03, on_note, VP 73 },
    /* 01 */ { "D#6", 0x03, on_note, VP 75 },
    /* 02 */ { "F#6", 0x03, on_note, VP 78 },
    /* 03 */ { "G#6", 0x03, on_note, VP 80 },
    /* 04 */ { "A#6", 0x03, on_note, VP 82 },
    /* 05 */ { "Mode", 0x14, on_cycle_mode, 0 },
    /* 06 */ { "Duration", 0x14, on_duration, 0 },
    /* 07 */ { "Rest", 0x13, on_note, VP 0 },
    /* 08 */ { "Up", 0x34, on_up, 0 },
    /* 09 */ { "C6", 0x03, on_note, VP 72 },
    /* 10 */ { "D6", 0x03, on_note, VP 74 },
    /* 11 */ { "E6", 0x03, on_note, VP 76 },
    /* 12 */ { "F6", 0x03, on_note, VP 77 },
    /* 13 */ { "G6", 0x03, on_note, VP 79 },
    /* 14 */ { "A6", 0x03, on_note, VP 81 },
    /* 15 */ { "B6", 0x03, on_note, VP 83 },
    /* 16 */ { "Sample", 0x14, on_sample, 0 },
    /* 17 */ { "Quality", 0x14, on_quality, 0 },
    /* 18 */ { "Style", 0x14, on_style, 0 },
    /* 19 */ { "Down", 0x34, on_down, 0 },
    /* 20 */ { "C#5", 0x03, on_note, VP 61 },
    /* 21 */ { "D#5", 0x03, on_note, VP 63 },
    /* 22 */ { "F#5", 0x03, on_note, VP 66 },
    /* 23 */ { "G#5", 0x03, on_note, VP 68 },
    /* 24 */ { "A#5", 0x03, on_note, VP 70 },
    /* 25 */ { "Key", 0x14, on_key_signature, 0 },
    /* 26 */ { "Time", 0x14, on_time_signature, 0 },
    /* 27 */ { "Tempo", 0x14, on_tempo, 0 },
    /* 28 */ { "Left", 0x34, on_left, 0 },
    /* 29 */ { "C5", 0x03, on_note, VP 60 },
    /* 30 */ { "D5", 0x03, on_note, VP 62 },
    /* 31 */ { "E5", 0x03, on_note, VP 64 },
    /* 32 */ { "F5", 0x03, on_note, VP 65 },
    /* 33 */ { "G5", 0x03, on_note, VP 67 },
    /* 34 */ { "A5", 0x03, on_note, VP 69 },
    /* 35 */ { "B5", 0x03, on_note, VP 71 },
    /* 36 */ { "Menu+", 0x13, ysw_menu_on_open, (void*)menu_2 },
    /* 37 */ { "Delete", 0x14, on_delete, 0 },
    /* 38 */ { "Menu-", 0x13, ysw_menu_on_close, 0 },
    /* 39 */ { "Right", 0x14, on_right, 0 },
    /* 40 */ { NULL, 0, NULL, NULL },
};

void ysw_editor_create_task(ysw_bus_h bus, zm_music_t *music, ysw_editor_lvgl_init lvgl_init)
{
    assert(bus);
    assert(music);
    assert(lvgl_init);

    assert(ysw_array_get_count(music->samples));
    assert(ysw_array_get_count(music->qualities) > DEFAULT_QUALITY);
    assert(ysw_array_get_count(music->styles));

    context_t *context = ysw_heap_allocate(sizeof(context_t));

    context->bus = bus;
    context->music = music;
    context->lvgl_init = lvgl_init;

    context->tone_sample = ysw_array_get(music->samples, 0);
    context->chord_sample = context->tone_sample;
    context->quality = ysw_array_get(music->qualities, DEFAULT_QUALITY);
    context->style = ysw_array_get(music->styles, DEFAULT_STYLE);
    context->duration = ZM_AS_PLAYED;

    context->advance = 2;
    context->position = 0;
    context->mode = YSW_EDITOR_MODE_TONE;

    context->passage = ysw_heap_allocate(sizeof(zm_passage_t));
    context->passage->beats = ysw_array_create(64);
    context->passage->key = ZM_KEY_C;
    context->passage->time = ZM_TIME_4_4;
    context->passage->tempo = ZM_TEMPO_100;

    context->menu = ysw_menu_create(base_menu, context);

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

