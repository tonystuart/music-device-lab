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
#include "ysw_heap.h"
#include "ysw_staff.h"
#include "ysw_task.h"
#include "ysw_ticks.h"
#include "zm_music.h"
#include "lvgl.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "EDITOR"

#if 1

// ysw_field

typedef struct {
    lv_cont_ext_t cont_ext; // base class data
    lv_obj_t *name;
    lv_obj_t *value;
} ysw_field_ext_t;

static lv_design_cb_t ysw_field_base_design;
static lv_signal_cb_t ysw_field_base_signal;

static lv_res_t ysw_field_on_signal(lv_obj_t *obj, lv_signal_t sign, void *param)
{
    return ysw_field_base_signal(obj, sign, param);
}

static lv_design_res_t ysw_field_on_design(lv_obj_t *obj, const lv_area_t *clip_area, lv_design_mode_t mode)
{
    return ysw_field_base_design(obj, clip_area, mode);
}

lv_obj_t *ysw_field_create(lv_obj_t *par)
{
    lv_obj_t *field = lv_cont_create(par, NULL);
    assert(field);

    ysw_field_ext_t *ext = lv_obj_allocate_ext_attr(field, sizeof(ysw_field_ext_t));
    assert(ext);

    if (ysw_field_base_signal == NULL) {
        ysw_field_base_signal = lv_obj_get_signal_cb(field);
    }

    if (ysw_field_base_design == NULL) {
        ysw_field_base_design = lv_obj_get_design_cb(field);
    }

    memset(ext, 0, sizeof(ysw_field_ext_t));

    lv_obj_set_signal_cb(field, ysw_field_on_signal);
    lv_obj_set_design_cb(field, ysw_field_on_design);

    ext->name = lv_label_create(field, NULL);
    lv_label_set_align(ext->name, LV_LABEL_ALIGN_CENTER);
    lv_obj_set_style_local_text_font(ext->name, 0, 0, &lv_font_unscii_8);

    ext->value = lv_label_create(field, NULL);
    lv_label_set_align(ext->value, LV_LABEL_ALIGN_CENTER);

    lv_obj_set_style_local_pad_top(field, 0, 0, 2);
    lv_obj_set_style_local_pad_left(field, 0, 0, 2);

    lv_cont_set_fit(field, LV_FIT_TIGHT);
    lv_cont_set_layout(field, LV_LAYOUT_COLUMN_LEFT);

    return field;
}

void ysw_field_set_name(lv_obj_t *field, const char *name)
{
    ysw_field_ext_t *ext = lv_obj_get_ext_attr(field);
    lv_label_set_text(ext->name, name);
}

void ysw_field_set_value(lv_obj_t *field, const char *value)
{
    ysw_field_ext_t *ext = lv_obj_get_ext_attr(field);
    lv_label_set_text(ext->value, value);
}

// Header

typedef struct {
    lv_cont_ext_t cont_ext; // base class data
    lv_obj_t *mode_field;
    lv_obj_t *sample_field;
} ysw_header_ext_t;

lv_obj_t *ysw_header_create(lv_obj_t *par)
{
    lv_obj_t *header = lv_cont_create(par, NULL);
    assert(header);

    ysw_header_ext_t *ext = lv_obj_allocate_ext_attr(header, sizeof(ysw_header_ext_t));
    assert(ext);

    memset(ext, 0, sizeof(ysw_header_ext_t));

    ext->mode_field = ysw_field_create(header);
    ysw_field_set_name(ext->mode_field, "Mode");

    ext->sample_field = ysw_field_create(header);
    ysw_field_set_name(ext->sample_field, "Sample");

    lv_cont_set_layout(header, LV_LAYOUT_PRETTY_TOP);

    return header;
}

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

void ysw_header_set_mode(lv_obj_t *header, const char *name, const char *value)
{
    ysw_header_ext_t *ext = lv_obj_get_ext_attr(header);
    ysw_field_set_name(ext->mode_field, name);
    ysw_field_set_value(ext->mode_field, value);
}

void ysw_header_set_sample(lv_obj_t *header, const char *sample_name)
{
    ysw_header_ext_t *ext = lv_obj_get_ext_attr(header);
    ysw_field_set_value(ext->sample_field, sample_name);
}

// Footer

typedef struct {
    lv_cont_ext_t cont_ext; // base class data
    lv_obj_t *key_field;
    lv_obj_t *time_field;
    lv_obj_t *tempo_field;
    lv_obj_t *duration_field;
} ysw_footer_ext_t;

lv_obj_t *ysw_footer_create(lv_obj_t *par)
{
    lv_obj_t *footer = lv_cont_create(par, NULL);
    assert(footer);

    ysw_footer_ext_t *ext = lv_obj_allocate_ext_attr(footer, sizeof(ysw_footer_ext_t));
    assert(ext);

    memset(ext, 0, sizeof(ysw_footer_ext_t));

    ext->key_field = ysw_field_create(footer);
    ysw_field_set_name(ext->key_field, "Key");

    ext->time_field = ysw_field_create(footer);
    ysw_field_set_name(ext->time_field, "Time");

    ext->tempo_field = ysw_field_create(footer);
    ysw_field_set_name(ext->tempo_field, "Tempo");

    ext->duration_field = ysw_field_create(footer);
    ysw_field_set_name(ext->duration_field, "Duration");

    lv_cont_set_layout(footer, LV_LAYOUT_PRETTY_TOP);

    return footer;
}

void ysw_footer_set_key(lv_obj_t *footer, zm_key_signature_x key_index)
{
    const zm_key_signature_t *key_signature = zm_get_key_signature(key_index);
    ysw_footer_ext_t *ext = lv_obj_get_ext_attr(footer);
    ysw_field_set_value(ext->key_field, key_signature->label);
}

void ysw_footer_set_time(lv_obj_t *footer, zm_time_signature_x time_index)
{
    const zm_time_signature_t *time_signature = zm_get_time_signature(time_index);
    ysw_footer_ext_t *ext = lv_obj_get_ext_attr(footer);
    ysw_field_set_value(ext->time_field, time_signature->name);
}

void ysw_footer_set_tempo(lv_obj_t *footer, zm_tempo_t tempo_index)
{
    const zm_tempo_signature_t *tempo_signature = zm_get_tempo_signature(tempo_index);
    ysw_footer_ext_t *ext = lv_obj_get_ext_attr(footer);
    ysw_field_set_value(ext->tempo_field, tempo_signature->label);
}

void ysw_footer_set_duration(lv_obj_t *footer, zm_duration_t duration)
{
    const char *value = NULL;
    switch (duration) {
        default:
        case ZM_AS_PLAYED:
            value = "As Played";
            break;
        case ZM_SIXTEENTH:
            value = "1/16";
            break;
        case ZM_EIGHTH:
            value = "1/8";
            break;
        case ZM_QUARTER:
            value = "1/4";
            break;
        case ZM_HALF:
            value = "1/2";
            break;
        case ZM_WHOLE:
            value = "1/1";
            break;
    }
    ysw_footer_ext_t *ext = lv_obj_get_ext_attr(footer);
    ysw_field_set_value(ext->duration_field, value);
}

#endif

// Layout of keycodes generated by keyboard:
//
//    0,  1,      2,  3,  4,      5,  6,  7,  8,
//  9, 10, 11, 12, 13, 14, 15,   16, 17, 18, 19,
//   20, 21,     22, 23, 24,     25, 26, 27, 28,
// 29, 30, 31, 32, 33, 34, 35,   36, 37, 38, 39,

// Mapping of keycodes to note values and keypad values:

static const uint8_t key_map[] = {
    /* 1 */   13, 15,     18, 20, 22,     24, 25, 26, 27,
    /* 2 */ 12, 14, 16, 17, 19, 21, 23,   28, 29, 30, 31,
    /* 3 */    1,  3,      6,  8, 10,     32, 33, 34, 35,
    /* 4 */  0,  2,  4,  5,  7,  9, 11,   36, 37, 38, 39,
};

#define KEY_MAP_SZ (sizeof(key_map) / sizeof(key_map[0]))

#define YSW_EDITOR_NOTES 24

#define YSW_EDITOR_MODE 24
#define YSW_EDITOR_KEY 25
#define YSW_EDITOR_TIME 26

#define YSW_EDITOR_TEMPO 28

// chords
#define YSW_EDITOR_QUALITY 29
#define YSW_EDITOR_STYLE 30

// notes
#define YSW_EDITOR_REST 29
#define YSW_EDITOR_DURATION 30

#define YSW_EDITOR_DELETE 32
#define YSW_EDITOR_SAMPLE 33

#define YSW_EDITOR_UP 27
#define YSW_EDITOR_DOWN 31
#define YSW_EDITOR_LEFT 35
#define YSW_EDITOR_RIGHT 39

// 0 % 2 = 0
// 1 % 2 = 1 C
// 2 % 2 = 0
// 3 % 2 = 1 E
// 4 % 2 = 0 
// 5 % 2 = 1 G
// 6 % 2 = 0

// position / 2 >= count ? space-after-note : position % 2 ? note index / 2 : space-before-note

typedef struct {
    ysw_bus_h bus;
    zm_music_t *music;
    zm_passage_t *passage;
    zm_sample_t *tone_sample;
    zm_sample_t *chord_sample;
    zm_quality_t *quality;
    zm_style_t *style;
    zm_duration_t duration;

    uint8_t advance;
    uint32_t position;
    ysw_editor_mode_t mode;

    lv_obj_t *container;
    lv_obj_t *header;
    lv_obj_t *staff;
    lv_obj_t *footer;
} context_t;

static void display_mode(context_t *context)
{
    char value[32] = {};
    uint32_t beat_count = ysw_array_get_count(context->passage->beats);
    if (beat_count) {
        uint32_t beat_index = context->position / 2;
        if (beat_index >= beat_count) {
            beat_index = beat_count - 1;
        }
        if (context->mode == YSW_EDITOR_MODE_TONE) {
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
        if (context->mode == YSW_EDITOR_MODE_CHORD) {
            zm_beat_t *beat = ysw_array_get(context->passage->beats, beat_index);
            zm_note_t root = beat->chord.root;
            if (root) {
                const char *name = zm_get_note_name(root);
                snprintf(value, sizeof(value), "%s %s %s", name,
                        beat->chord.quality->name,
                        beat->chord.style->name);
            }
        }
        ysw_header_set_mode(context->header, modes[context->mode], value);
    }
}

static void display_sample(context_t *context)
{
    ysw_header_set_sample(context->header, context->tone_sample->name);
}

static void cycle_editor_mode(context_t *context)
{
    context->mode = (context->mode + 1) % YSW_EDITOR_MODE_COUNT;
    display_mode(context);
}

static void cycle_sample(context_t *context)
{
    ysw_array_t *samples = context->music->samples;
    zm_sample_x sample_count = ysw_array_get_count(samples);
    zm_sample_x sample_index = ysw_array_find(samples, context->tone_sample);
    sample_index = (sample_index + 1) % sample_count;
    context->tone_sample = ysw_array_get(samples, sample_index);
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

static void cycle_tempo_signature(context_t *context)
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
        uint32_t beat_index = context->position / 2;
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

static void process_note(context_t *context, ysw_event_key_up_t *event)
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

    uint8_t value = key_map[event->key];
    zm_bpm_x bpm = zm_tempo_to_bpm(context->passage->tempo);
    if (context->mode == YSW_EDITOR_MODE_TONE) {
        beat->tone.note = value == YSW_EDITOR_REST ? 0 : 60 + value;
        if (context->duration == ZM_AS_PLAYED) {
            beat->tone.duration = ysw_millis_to_ticks(event->duration, bpm);
        } else {
            beat->tone.duration = context->duration;
        }
    } else if (context -> mode == YSW_EDITOR_MODE_CHORD) {
        if (is_new_beat) {
            beat->tone.duration = ZM_QUARTER;
        }
        beat->chord.root = 60 + value;
        beat->chord.quality = context->quality;
        beat->chord.style = context->style;
        beat->chord.duration = ZM_HALF;
        ysw_array_t *notes = ysw_array_create(16);
        zm_sample_x sample_index = ysw_array_find(context->music->samples, context->chord_sample);
        zm_render_step(notes, &beat->chord, 0, 1, sample_index);
        ysw_event_fire_play(context->bus, notes, bpm); // TODO: consider supplying origin or use channel
    } else if (context -> mode == YSW_EDITOR_MODE_DRUM) {
    }

    uint32_t beat_count = ysw_array_get_count(context->passage->beats);
    context->position = min(context->position + context->advance, beat_count * 2);
    ysw_staff_update_all(context->staff, context->position);
    display_mode(context);
}

static void process_delete(context_t *context)
{
    if (context->position % 2 == 1) {
        uint32_t beat_index = context->position / 2;
        zm_beat_t *beat = ysw_array_remove(context->passage->beats, beat_index);
        ysw_heap_free(beat);
        uint32_t beat_count = ysw_array_get_count(context->passage->beats);
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

static void process_left(context_t *context)
{
    if (context->position > 0) {
        context->position--;
        ysw_staff_update_position(context->staff, context->position);
        display_mode(context);
    }
}

static void process_right(context_t *context)
{
    if (context->position < (ysw_array_get_count(context->passage->beats) * 2)) {
        context->position++;
        ysw_staff_update_position(context->staff, context->position);
        display_mode(context);
    }
}

static void on_key_down(context_t *context, ysw_event_key_down_t *event)
{
    assert(event->key < KEY_MAP_SZ);
    if (context->mode == YSW_EDITOR_MODE_TONE) {
        uint8_t value = key_map[event->key];
        if (value < YSW_EDITOR_NOTES) {
            ysw_event_program_change_t program_change = {
                .channel = 0,
                .program = ysw_array_find(context->music->samples, context->tone_sample),
            };
            ysw_event_fire_program_change(context->bus, YSW_ORIGIN_EDITOR, &program_change);
            ysw_event_note_on_t note_on = {
                .channel = 0,
                .midi_note = 60 + value,
                .velocity = 80,
            };
            ysw_event_fire_note_on(context->bus, YSW_ORIGIN_EDITOR, &note_on);
        }
    }
}

static void on_key_up(context_t *context, ysw_event_key_up_t *event)
{
    assert(event->key < KEY_MAP_SZ);
    uint8_t value = key_map[event->key];
    if (context->mode == YSW_EDITOR_MODE_TONE) {
        if (value < YSW_EDITOR_NOTES) {
            ysw_event_note_off_t note_off = {
                .channel = 0,
                .midi_note = 60 + value,
            };
            ysw_event_fire_note_off(context->bus, YSW_ORIGIN_EDITOR, &note_off);
        }
    }
    if (value < YSW_EDITOR_NOTES || value == YSW_EDITOR_REST) {
        process_note(context, event);
    }
}

static void on_key_pressed(context_t *context, ysw_event_key_pressed_t *event)
{
    assert(event->key < KEY_MAP_SZ);
    uint8_t value = key_map[event->key];
    ESP_LOGD(TAG, "on_key_pressed key=%d, value=%d", event->key, value);
    switch (value) {
        case YSW_EDITOR_MODE:
            cycle_editor_mode(context);
            break;
        case YSW_EDITOR_SAMPLE:
            cycle_sample(context);
            break;
        case YSW_EDITOR_DURATION:
            cycle_duration(context);
            break;
        case YSW_EDITOR_KEY:
            cycle_key_signature(context);
            break;
        case YSW_EDITOR_TIME:
            cycle_time_signature(context);
            break;
        case YSW_EDITOR_TEMPO:
            cycle_tempo_signature(context);
            break;
        case YSW_EDITOR_DELETE:
            process_delete(context);
            break;
        case YSW_EDITOR_LEFT:
            process_left(context);
            break;
        case YSW_EDITOR_RIGHT:
            process_right(context);
            break;
        default:
            break;
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
                on_key_down(context, &event->key_down);
                break;
            case YSW_EVENT_KEY_UP:
                on_key_up(context, &event->key_up);
                break;
            case YSW_EVENT_KEY_PRESSED:
                on_key_pressed(context, &event->key_pressed);
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

#define QUALITY 2 // TODO: make 0 default or search by name

void ysw_editor_create_task(ysw_bus_h bus, zm_music_t *music)
{
    assert(ysw_array_get_count(music->samples));
    assert(ysw_array_get_count(music->qualities) > QUALITY);
    assert(ysw_array_get_count(music->styles));

    context_t *context = ysw_heap_allocate(sizeof(context_t));

    context->bus = bus;
    context->music = music;
    context->tone_sample = ysw_array_get(music->samples, 0);
    context->chord_sample = context->tone_sample;
    context->quality = ysw_array_get(music->qualities, QUALITY);
    context->style = ysw_array_get(music->styles, 0);
    context->duration = ZM_AS_PLAYED;

    context->advance = 2;
    context->position = 0;
    context->mode = YSW_EDITOR_MODE_TONE;

    context->passage = ysw_heap_allocate(sizeof(zm_passage_t));
    context->passage->beats = ysw_array_create(64);
    context->passage->key = ZM_KEY_C;
    context->passage->time = ZM_TIME_4_4;
    context->passage->tempo = ZM_TEMPO_100;

    context->container = lv_obj_create(lv_scr_act(), NULL);

    lv_obj_set_style_local_bg_color(context->container, 0, 0, LV_COLOR_MAROON);
    lv_obj_set_style_local_bg_opa(context->container, 0, 0, LV_OPA_100);

    lv_obj_set_style_local_text_color(context->container, 0, 0, LV_COLOR_YELLOW);
    lv_obj_set_style_local_text_opa(context->container, 0, 0, LV_OPA_100);

    assert(context->container);
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

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.event_handler = process_event;
    config.caller_context = context;
    config.wait_millis = 5;
    config.priority = YSW_TASK_DEFAULT_PRIORITY + 1;

    ysw_task_h task = ysw_task_create(&config);

    ysw_task_subscribe(task, YSW_ORIGIN_KEYBOARD);
    ysw_task_subscribe(task, YSW_ORIGIN_SEQUENCER);
}

