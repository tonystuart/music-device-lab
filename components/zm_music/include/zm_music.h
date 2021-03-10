// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_array.h"
#include "ysw_common.h"
#include "ysw_heap.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"

#define ZM_MF_PARTITION "/spiffs"
#define ZM_MF_CSV ZM_MF_PARTITION "/music.csv"
#define ZM_MF_TEMP ZM_MF_PARTITION "/music.tmp"
#define ZM_NAME_SZ 32

typedef uint8_t zm_small_t;
typedef uint16_t zm_medium_t;
typedef uint32_t zm_large_t;

typedef bool zm_yesno_t;

// alphabetical order by type

typedef int8_t zm_distance_t;
typedef int32_t zm_range_x;

typedef uint8_t zm_bank_x;
typedef uint8_t zm_beat_x;
typedef uint8_t zm_bpm_x;
typedef uint8_t zm_channel_x;
typedef uint8_t zm_distance_x;
typedef uint8_t zm_gm_x;
typedef uint8_t zm_key_signature_x;
typedef uint8_t zm_note_t;
typedef uint8_t zm_octave_t;
typedef uint8_t zm_percent_x;
typedef uint8_t zm_program_x;
typedef uint8_t zm_stroke_x;
typedef uint8_t zm_tempo_x;
typedef uint8_t zm_tie_x;
typedef uint8_t zm_velocity_x;

typedef uint16_t zm_chord_style_x;
typedef uint16_t zm_chord_type_x;
typedef uint16_t zm_composition_x;
typedef uint16_t zm_measure_x;
typedef uint16_t zm_note_x;
typedef uint16_t zm_part_x;
typedef uint16_t zm_section_x;

typedef uint32_t zm_step_x;
typedef uint32_t zm_time_x;

typedef struct {
    zm_time_x clock;
} zm_settings_t;

typedef struct PACKED {
    const char *name;
    const bool major;
    const uint8_t sharps;
    const uint8_t flats;
    const uint8_t sharp_index[7];
    const uint8_t flat_index[7];
    const char *label;
    const uint8_t tonic;
    const uint8_t tonic_semis;
} zm_key_signature_t;

typedef struct PACKED {
    int8_t semitone[12];
} zm_scale_t;

extern const zm_scale_t zm_scales[];

typedef enum {
    ZM_AS_PLAYED = 0,
    ZM_SIXTEENTH = 64,
    ZM_EIGHTH = 128,
    ZM_QUARTER = 256,
    ZM_DOTTED_QUARTER = ZM_QUARTER + (ZM_QUARTER / 2),
    ZM_HALF = 512,
    ZM_DOTTED_HALF = ZM_HALF + (ZM_HALF / 2),
    ZM_WHOLE = 1024,
} zm_duration_t;

typedef enum {
    ZM_ONE_PER_MEASURE = 1,
    ZM_TWO_PER_MEASURE = 2,
    ZM_THREE_PER_MEASURE = 3,
    ZM_FOUR_PER_MEASURE = 4,
} zm_chord_frequency_t;

typedef enum {
    ZM_TIME_2_2,
    ZM_TIME_2_4,
    ZM_TIME_3_4,
    ZM_TIME_4_4,
    ZM_TIME_6_8,
} zm_time_signature_x;

typedef struct {
    const char *name;
    const zm_small_t count;
    const zm_duration_t unit;
    const zm_time_x ticks_per_measure;
} zm_time_signature_t;

typedef struct {
    char *name;
    char *label;
    ysw_array_t *distances;
} zm_chord_type_t;

typedef struct {
    zm_distance_x distance_index;
    zm_velocity_x velocity;
    zm_medium_t start;
    zm_medium_t duration;
} zm_sound_t;

typedef struct {
    char *name;
    char *label;
    zm_distance_x distance_count;
    ysw_array_t *sounds;   
} zm_chord_style_t;

typedef struct {
    zm_note_t root;
    zm_chord_type_t *type;
    zm_chord_style_t *style;
    zm_duration_t duration;
    zm_chord_frequency_t frequency;
} zm_chord_t;

typedef struct {
    zm_note_t note; // Use 0 for rest
    zm_duration_t duration;
    zm_tie_x tie; // tie/portamento
} zm_melody_t;

typedef struct {
    zm_time_x start;
    zm_note_t surface;
    zm_velocity_x velocity;
} zm_stroke_t;

typedef struct {
    char *name;
    char *label;
    ysw_array_t *strokes;
} zm_beat_t;

typedef struct {
    zm_beat_t *beat;
    zm_note_t surface;
    zm_duration_t cadence;
} zm_rhythm_t;

typedef enum {
    ZM_STEP_END_OF_MEASURE = 0x0001,
} zm_step_flags_t;

typedef struct {
    zm_time_x start;
    zm_measure_x measure;
    zm_step_flags_t flags;
    zm_melody_t melody;
    zm_chord_t chord;
    zm_rhythm_t rhythm;
} zm_step_t;

typedef struct {
    char *name;
    zm_tempo_x tempo;
    zm_key_signature_x key;
    zm_time_signature_x time;
    zm_time_x tlm;
    ysw_array_t *steps;
    zm_program_x melody_program;
    zm_program_x chord_program;
} zm_section_t;

typedef enum {
    ZM_WHEN_TYPE_WITH,
    ZM_WHEN_TYPE_AFTER,
} zm_when_type_t;

typedef struct {
    zm_when_type_t type;
    zm_medium_t part_index;
} zm_when_t;

typedef enum {
    ZM_FIT_ONCE,
    ZM_FIT_LOOP,
} zm_fit_t;

typedef struct {
    zm_large_t begin_time;
    zm_large_t end_time;
} zm_part_time_t;

typedef struct {
    char *name;
    zm_small_t bpm;
    // key signature
    // time signature
    ysw_array_t *parts;
} zm_composition_t;

typedef struct {
    zm_section_t *section;
    zm_percent_x percent_volume;
    zm_when_t when;
    zm_fit_t fit;
} zm_part_t;

typedef struct {
    zm_settings_t settings;
    ysw_array_t *chord_types;
    ysw_array_t *chord_styles;
    ysw_array_t *beats;
    ysw_array_t *sections;
    ysw_array_t *compositions;
} zm_music_t;

typedef struct {
    zm_range_x first;
    zm_range_x last;
} zm_range_t;

void zm_section_free(zm_section_t *section);
void zm_section_delete(zm_music_t *music, zm_section_t *section);
void zm_music_free(zm_music_t *music);
zm_music_t *zm_parse_file(FILE *file);
zm_music_t *zm_load_music(void);
void zm_save_music(zm_music_t *music);

void *zm_load_sample(const char* name, uint16_t *byte_count);

int zm_note_compare(const void *left, const void *right);

zm_time_x zm_get_step_duration(zm_step_t *step, zm_time_x ticks_per_measure);
void zm_recalculate_section(zm_section_t *section);

void zm_render_melody(ysw_array_t *notes, zm_melody_t *melody, zm_time_x melody_start, zm_channel_x channel, zm_program_x program_index, zm_tie_x tie);
void zm_render_chord(ysw_array_t *notes, zm_chord_t *chord, zm_time_x chord_start, zm_channel_x channel, zm_program_x program_index);
ysw_array_t *zm_render_step(zm_music_t *m, zm_section_t *p, zm_step_t *d, zm_channel_x bc);
zm_time_x zm_render_section_notes(ysw_array_t *notes, zm_music_t *music, zm_section_t *section, zm_time_x start_time, zm_channel_x base_channel);
ysw_array_t *zm_render_section(zm_music_t *music, zm_section_t *section, zm_channel_x base_channel);
ysw_array_t *zm_render_composition(zm_music_t *music, zm_composition_t *composition, zm_channel_x base_channel);

const zm_key_signature_t *zm_get_key_signature(zm_key_signature_x key_index);
zm_key_signature_x zm_get_next_key_index(zm_key_signature_x key_index);

const zm_time_signature_t *zm_get_time_signature(zm_time_signature_x time_index);

zm_duration_t zm_round_duration(zm_duration_t duration, uint8_t *index, bool *is_dotted);
zm_duration_t zm_get_next_dotted_duration(zm_duration_t duration, int direction);
const char *zm_get_duration_label(zm_duration_t duration);
zm_duration_t zm_get_next_duration(zm_duration_t duration);

zm_section_t *zm_create_section(zm_music_t *music);

zm_section_t *zm_create_duplicate_section(zm_section_t *section);

void zm_copy_section(zm_section_t *to_section, zm_section_t *from_section);

void zm_rename_section(zm_section_t *section, const char *name);

bool zm_sections_equal(zm_section_t *left, zm_section_t *right);

ysw_array_t *zm_get_section_references(zm_music_t *music, zm_section_t *section);

bool zm_transpose_section(zm_section_t *section, zm_range_t *range, uint8_t delta);

// See https://en.wikipedia.org/wiki/C_(musical_note) for octave designation

static inline const char *zm_get_note_name(zm_note_t note)
{
    static const char *note_names[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
    };
    return note_names[note % 12];
}

static inline zm_time_x zm_get_ticks_per_measure(zm_time_signature_x t)
{
    return zm_get_time_signature(t)->ticks_per_measure;
}

void zm_generate_scales();
