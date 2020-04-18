// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "esp_log.h"
#include "ysw_array.h"
#include "ysw_song.h"

typedef enum {
    I = 1,
    ii,
    iii,
    IV,
    V,
    vi,
    vii
} ysw_chord_number_t;

typedef enum {
    TONIC = 1,
    SUPERTONIC,
    MEDIANT,
    SUBDOMINANT,
    DOMINANT,
    SUBMEDIANT,
    SUBTONIC,
} ysw_degree_t;

typedef struct {
    uint32_t time;
    uint32_t duration;
    int8_t degree;
    uint8_t velocity;
} ysw_chord_note_t;

typedef struct {
    char *name;
    ysw_array_t *chord_notes;
    uint32_t duration;
} ysw_chord_style_t;

typedef struct {
    ysw_chord_number_t chord_number;
    ysw_chord_style_t *chord_style;
} ysw_chord_t;

typedef struct {
    ysw_array_t *chords;
    ysw_chord_style_t *chord_style;
    uint8_t tonic;
    uint8_t instrument;
    uint8_t percent_tempo;
} ysw_clip_t;

note_t *ysw_clip_get_notes(ysw_clip_t *clip);
uint32_t ysw_get_note_count(ysw_clip_t *clip);
void ysw_clip_set_percent_tempo(ysw_clip_t *clip, uint8_t percent_tempo);
void ysw_clip_set_instrument(ysw_clip_t *clip, uint8_t instrument);
void ysw_clip_set_tonic(ysw_clip_t *clip, uint8_t tonic);
void ysw_clip_set_chord_style(ysw_clip_t *clip, ysw_chord_style_t *chord_style);
int ysw_clip_add_chord(ysw_clip_t *clip, uint8_t chord_number);
int ysw_clip_add_chord_with_style(ysw_clip_t *clip, uint8_t chord_number, ysw_chord_style_t *chord_style);
void ysw_clip_free(ysw_clip_t *clip);
ysw_clip_t *ysw_clip_create();
void ysw_chord_note_free(ysw_chord_note_t *ysw_chord_note);
ysw_chord_note_t *ysw_chord_note_create(uint8_t degree, uint8_t velocity, uint32_t time, uint32_t duration);
void ysw_chord_style_set_duration(ysw_chord_style_t *chord_style, uint32_t duration);
int ysw_chord_style_add_note(ysw_chord_style_t *chord_style, ysw_chord_note_t *chord_note);
void ysw_chord_style_free(ysw_chord_style_t *chord_style);
ysw_chord_style_t *ysw_chord_style_create();

