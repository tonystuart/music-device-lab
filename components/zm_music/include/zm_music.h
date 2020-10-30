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

typedef uint8_t zm_note_t;
typedef uint8_t zm_semitone_t;

typedef uint8_t zm_small_t;
typedef uint16_t zm_medium_t;
typedef uint32_t zm_large_t;

typedef bool zm_yesno_t;

typedef zm_medium_t zm_index_t;

typedef uint8_t zm_bpm_x;
typedef uint8_t zm_key_x;

typedef uint16_t zm_quality_x;
typedef uint16_t zm_style_x;

typedef struct {
    const char *name;
    const uint8_t sharps;
    const uint8_t flats;
    const uint8_t sharp_indexes[7];
    const uint8_t flat_index[7];
} zm_key_t;

typedef struct {
    zm_key_x index;
    char name[32];
} zm_key_signature_t;

typedef enum {
    ZM_TIME_2_2,
    ZM_TIME_2_4,
    ZM_TIME_3_4,
    ZM_TIME_4_4,
    ZM_TIME_3_2,
    ZM_TIME_6_8,
} zm_time_t;

typedef enum {
    ZM_SIXTEENTH = 64,
    ZM_EIGHTH = 128,
    ZM_QUARTER = 256,
    ZM_HALF = 512,
    ZM_WHOLE = 1024,
} zm_duration_t;

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
    ysw_array_t *semitones;
} zm_quality_t;

typedef struct {
    zm_medium_t semitone_index;
    zm_medium_t velocity;
    zm_medium_t start;
    zm_medium_t duration;
} zm_sound_t;

typedef struct {
    char *name;
    ysw_array_t *sounds;   
} zm_style_t;

typedef struct {
    zm_note_t root;
    zm_quality_t *quality;
    zm_style_t *style;
    zm_duration_t duration;
} zm_step_t;

typedef struct {
    char *name;
    zm_medium_t reppnt;
    zm_medium_t replen;
    zm_small_t volume;
    zm_pan_t pan;
} zm_sample_t;

typedef struct {
    char *name;
    zm_medium_t sample_index;
    ysw_array_t *steps;
} zm_pattern_t;

typedef enum {
    ZM_WHEN_TYPE_WITH,
    ZM_WHEN_TYPE_AFTER,
} zm_when_type_t;

typedef struct {
    zm_when_type_t type;
    zm_medium_t part_index;
} zm_when_t;

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
} zm_song_t;

typedef struct {
    zm_pattern_t *pattern;
    zm_when_t when;
    zm_fit_t fit;
} zm_part_t;

typedef struct {
    zm_note_t note; // Use 0 for rest
    zm_duration_t duration;
} zm_tone_t;

typedef struct {
    char *name;
    ysw_array_t *patterns;
    zm_duration_t duration;
} zm_rhythm_t;

typedef struct {
    zm_tone_t tone;
    zm_step_t chord;
    zm_rhythm_t rhythm;
} zm_beat_t;

typedef struct {
    char *name;
    zm_bpm_x bpm;
    zm_key_x key;
    zm_time_t time;
    ysw_array_t *beats;
} zm_passage_t;

typedef struct {
    ysw_array_t *samples;
    ysw_array_t *qualities;
    ysw_array_t *styles;
    ysw_array_t *patterns;
    ysw_array_t *songs;
} zm_music_t;

void zm_music_free(zm_music_t *music);
zm_music_t *zm_read_from_file(FILE *file);
zm_music_t *zm_read(void);
zm_large_t zm_render_pattern(ysw_array_t *notes, zm_pattern_t *pattern, zm_large_t start_time, zm_small_t channel);
ysw_array_t *zm_render_song(zm_song_t *song);
void zm_get_key_signature(zm_key_x key_index, zm_key_signature_t *key_signature);
zm_key_t *zm_get_key(zm_key_x key_index);
zm_key_x zm_get_next_key_index(zm_key_x key_index);
