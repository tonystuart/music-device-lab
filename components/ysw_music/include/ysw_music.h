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
} ysw_degree_t;

typedef enum {
    TONIC = 1,
    SUPERTONIC,
    MEDIANT,
    SUBDOMINANT,
    DOMINANT,
    SUBMEDIANT,
    SUBTONIC,
} ysw_role_t;

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
} ysw_chord_t;

typedef struct {
    ysw_degree_t degree;
    ysw_chord_t *chord;
} ysw_progression_chord_t;

typedef struct {
    char *name;
    ysw_array_t *chords;
    uint8_t tonic;
    uint8_t instrument;
    uint8_t percent_tempo;
} ysw_progression_t;

typedef struct {
    ysw_array_t *chords;
    ysw_array_t *progressions;
} ysw_music_t;

note_t *ysw_progression_get_notes(ysw_progression_t *progression);
uint32_t ysw_get_note_count(ysw_progression_t *progression);
void ysw_progression_set_percent_tempo(ysw_progression_t *progression, uint8_t percent_tempo);
void ysw_progression_set_instrument(ysw_progression_t *progression, uint8_t instrument);
void ysw_progression_set_tonic(ysw_progression_t *progression, uint8_t tonic);
int ysw_progression_add_chord(ysw_progression_t *progression, uint8_t degree, ysw_chord_t *chord);
void ysw_progression_free(ysw_progression_t *progression);
ysw_progression_t *ysw_progression_create(char *name, uint8_t tonic, uint8_t instrument, uint8_t bpm);
void ysw_chord_note_free(ysw_chord_note_t *ysw_chord_note);
ysw_chord_note_t *ysw_chord_note_create(int8_t degree, uint8_t velocity, uint32_t time, uint32_t duration);
void ysw_chord_set_duration(ysw_chord_t *chord, uint32_t duration);
int ysw_chord_add_note(ysw_chord_t *chord, ysw_chord_note_t *chord_note);
void ysw_chord_free(ysw_chord_t *chord);
ysw_chord_t *ysw_chord_create();

