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
#include "ysw_heap.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"

#define ZM_MF_PARTITION "/spiffs"
#define ZM_MF_CSV ZM_MF_PARTITION "/music.csv"
#define ZM_MF_TEMP ZM_MF_PARTITION "/music.tmp"

typedef uint8_t zm_small_t;
typedef uint16_t zm_medium_t;
typedef uint32_t zm_large_t;

typedef bool zm_yesno_t;

// alphabetical order by type

typedef int8_t zm_distance_t;

typedef uint8_t zm_beat_x;
typedef uint8_t zm_bpm_x;
typedef uint8_t zm_channel_x;
typedef uint8_t zm_distance_x;
typedef uint8_t zm_gm_x;
typedef uint8_t zm_note_t;
typedef uint8_t zm_patch_x;
typedef uint8_t zm_program_x;
typedef uint8_t zm_stroke_x;
typedef uint8_t zm_tie_x;
typedef uint8_t zm_velocity_x;

typedef uint16_t zm_measure_x;
typedef uint16_t zm_model_x;
typedef uint16_t zm_pattern_x;
typedef uint16_t zm_quality_x;
typedef uint16_t zm_sample_x;
typedef uint16_t zm_song_x;
typedef uint16_t zm_style_x;

typedef uint32_t zm_division_x;
typedef uint32_t zm_time_x;

typedef enum {
    ZM_KEY_C,
    ZM_KEY_G,
    ZM_KEY_D,
    ZM_KEY_A,
    ZM_KEY_E,
    ZM_KEY_B,
    ZM_KEY_F_SHARP,
    ZM_KEY_F,
    ZM_KEY_B_FLAT,
    ZM_KEY_E_FLAT,
    ZM_KEY_A_FLAT,
    ZM_KEY_D_FLAT,
    ZM_KEY_G_FLAT,
} zm_key_signature_x;

typedef struct {
    const char *name;
    const uint8_t sharps;
    const uint8_t flats;
    const uint8_t sharp_index[7];
    const uint8_t flat_index[7];
    const char *label;
} zm_key_signature_t;

typedef enum {
    ZM_AS_PLAYED = 0,
    ZM_SIXTEENTH = 64,
    ZM_EIGHTH = 128,
    ZM_QUARTER = 256,
    ZM_HALF = 512,
    ZM_WHOLE = 1024,
} zm_duration_t;

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
} zm_time_signature_t;

typedef enum {
    ZM_TEMPO_30,
    ZM_TEMPO_50,
    ZM_TEMPO_80,
    ZM_TEMPO_100,
    ZM_TEMPO_120,
    ZM_TEMPO_150,
    ZM_TEMPO_180,
} zm_tempo_t;

typedef struct {
    const char *name;
    const char *label;
    const zm_bpm_x bpm;
} zm_tempo_signature_t;

typedef enum {
    ZM_FIT_ONCE,
    ZM_FIT_LOOP,
} zm_fit_t;

typedef enum {
    ZM_PAN_LEFT,
    ZM_PAN_CENTER,
    ZM_PAN_RIGHT,
} zm_pan_t;

typedef struct {
    char *name;
    zm_medium_t reppnt;
    zm_medium_t replen;
    zm_small_t volume;
    zm_pan_t pan;
} zm_sample_t;

typedef enum {
    ZM_PROGRAM_NOTE,
    ZM_PROGRAM_BEAT,
} zm_program_type_t;

typedef struct {
    char *name;
    zm_program_type_t type;
    zm_gm_x gm;
    ysw_array_t *patches;
} zm_program_t;

typedef struct {
    zm_note_t up_to;
    zm_sample_t *sample;
    char *name;
} zm_patch_t;

typedef struct {
    char *name;
    char *label;
    ysw_array_t *distances;
} zm_quality_t;

typedef struct {
    zm_distance_x distance_index;
    zm_velocity_x velocity;
    zm_medium_t start;
    zm_medium_t duration;
} zm_sound_t;

typedef struct {
    char *name;
    zm_distance_x distance_count;
    ysw_array_t *sounds;   
} zm_style_t;

typedef struct {
    zm_note_t root;
    zm_quality_t *quality;
    zm_style_t *style;
    zm_duration_t duration;
} zm_chord_t;

typedef struct {
    char *name;
    zm_medium_t sample_index;
    ysw_array_t *chords;
} zm_model_t;

typedef enum {
    ZM_WHEN_TYPE_WITH,
    ZM_WHEN_TYPE_AFTER,
} zm_when_type_t;

typedef struct {
    zm_when_type_t type;
    zm_medium_t role_index;
} zm_when_t;

typedef struct {
    zm_large_t begin_time;
    zm_large_t end_time;
} zm_role_time_t;

typedef struct {
    char *name;
    zm_small_t bpm;
    // key signature
    // time signature
    ysw_array_t *roles;
} zm_song_t;

typedef struct {
    zm_model_t *model;
    zm_when_t when;
    zm_fit_t fit;
} zm_role_t;

typedef struct {
    zm_note_t note; // Use 0 for rest
    zm_duration_t duration;
    zm_time_x articulation;
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
    zm_note_t surface;
    zm_beat_t *beat;
} zm_rhythm_t;

typedef enum {
    ZM_DIVISION_END_OF_MEASURE = 0x0001,
} zm_division_flags_t;

typedef struct {
    zm_time_x start;
    zm_measure_x measure;
    zm_division_flags_t flags;
    zm_melody_t melody;
    zm_chord_t chord;
    zm_rhythm_t rhythm;
} zm_division_t;

typedef struct {
    char *name;
    zm_tempo_t tempo;
    zm_key_signature_x key;
    zm_time_signature_x time;
    ysw_array_t *divisions;
    zm_program_t *melody_program;
    zm_program_t *chord_program;
    zm_program_t *rhythm_program;
} zm_pattern_t;

typedef struct {
    ysw_array_t *samples;
    ysw_array_t *programs;
    ysw_array_t *qualities;
    ysw_array_t *styles;
    ysw_array_t *beats;
    ysw_array_t *patterns;
    ysw_array_t *models;
    ysw_array_t *songs;
} zm_music_t;

void zm_music_free(zm_music_t *music);
zm_music_t *zm_parse_file(FILE *file);
zm_music_t *zm_load_music(void);

void *zm_load_sample(const char* name, uint16_t *word_count);

int zm_note_compare(const void *left, const void *right);

void zm_render_melody(ysw_array_t *notes, zm_melody_t *melody, zm_time_x melody_start, zm_channel_x channel, zm_program_x program_index, zm_tie_x tie);
void zm_render_chord(ysw_array_t *notes, zm_chord_t *chord, zm_time_x chord_start, zm_channel_x channel, zm_program_x program_index);
zm_large_t zm_render_model(ysw_array_t *notes, zm_model_t *model, zm_large_t start_time, zm_small_t channel);
ysw_array_t *zm_render_song(zm_song_t *song);
ysw_array_t *zm_render_division(zm_music_t *m, zm_pattern_t *p, zm_division_t *d, zm_channel_x bc);
ysw_array_t *zm_render_pattern(zm_music_t *music, zm_pattern_t *pattern, zm_channel_x base_channel);

const zm_key_signature_t *zm_get_key_signature(zm_key_signature_x key_index);
zm_key_signature_x zm_get_next_key_index(zm_key_signature_x key_index);

zm_time_signature_x zm_get_next_time_index(zm_time_signature_x time_index);
const zm_time_signature_t *zm_get_time_signature(zm_time_signature_x time_index);

zm_tempo_t zm_get_next_tempo_index(zm_tempo_t tempo_index);
const zm_tempo_signature_t *zm_get_tempo_signature(zm_tempo_t tempo_index);
zm_bpm_x zm_tempo_to_bpm(zm_tempo_t tempo);

zm_duration_t zm_round_duration(zm_duration_t duration, uint8_t *index, bool *is_dotted);
const char *zm_get_duration_label(zm_duration_t duration);
zm_duration_t zm_get_next_duration(zm_duration_t duration);

zm_patch_t *zm_get_patch(ysw_array_t *patches, zm_note_t midi_note);

// See https://en.wikipedia.org/wiki/C_(musical_note) for octave designation

static inline const char *zm_get_note_name(zm_note_t note)
{
    static const char *note_names[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
    };
    return note_names[note % 12];
}

