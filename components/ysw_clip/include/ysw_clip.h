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
    I,
    ii,
    iii,
    IV,
    V,
    vi,
    vii
} ysw_chord_t;

typedef enum {
    TONIC,
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
    ysw_degree_t degree;
    uint8_t velocity;
} ysw_chord_note_t;

typedef struct {
    ysw_array_t *chord_notes;
    ysw_array_t *chords;
    ysw_array_t *melody;
    uint8_t tonic;
    uint8_t instrument;
    uint8_t percent_tempo;
    uint32_t measure_duration;
} ysw_clip_t;

ysw_clip_t *ysw_clip_create();
int ysw_clip_add_chord_note(ysw_clip_t *s, ysw_chord_note_t *chord_note);
int ysw_clip_add_chord(ysw_clip_t *s, ysw_chord_t chord);
void ysw_clip_free(ysw_clip_t *ysw_clip);
ysw_chord_note_t *ysw_chord_note_create(uint8_t degree, uint8_t velocity, uint32_t time, uint32_t duration);
void ysw_chord_note_free(ysw_chord_note_t *ysw_chord_note);
void ysw_clip_set_instrument(ysw_clip_t *sequence, uint8_t instrument);
void ysw_clip_set_percent_tempo(ysw_clip_t *sequence, uint8_t percent_tempo);
void ysw_clip_set_measure_duration(ysw_clip_t *sequence, uint32_t measure_duration);
note_t *ysw_clip_get_notes(ysw_clip_t *sequence);
uint32_t ysw_get_note_count(ysw_clip_t *sequence);
