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
#include "ysw_song.h"
#include "ysw_chord.h"

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
    uint32_t start;
    uint32_t duration;
    int8_t degree;
    uint8_t velocity;
} ysw_chord_note_t;

typedef struct {
    char *name;
    ysw_array_t *chord_notes;
    uint32_t duration;
} ysw_chord_t;

static inline uint32_t ysw_chord_get_note_count(ysw_chord_t *chord)
{
    return ysw_array_get_count(chord->chord_notes);
}

static inline ysw_chord_note_t *ysw_chord_get_chord_note(ysw_chord_t *chord, uint32_t index)
{
    return ysw_array_get(chord->chord_notes, index);
}

void ysw_chord_note_free(ysw_chord_note_t *ysw_chord_note);
ysw_chord_note_t *ysw_chord_note_create(int8_t degree, uint8_t velocity, uint32_t start, uint32_t duration);
void ysw_chord_set_duration(ysw_chord_t *chord, uint32_t duration);
uint32_t ysw_chord_add_note(ysw_chord_t *chord, ysw_chord_note_t *chord_note);
void ysw_chord_free(ysw_chord_t *chord);
ysw_chord_t *ysw_chord_create();
void ysw_chord_dump(ysw_chord_t *chord, char *tag);
