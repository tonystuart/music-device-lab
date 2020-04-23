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
#include "ysw_chord.h"

typedef struct {
    ysw_degree_t degree;
    ysw_chord_t *chord;
} ysw_slot_t;

typedef struct {
    char *name;
    ysw_array_t *slots;
    uint8_t tonic;
    uint8_t instrument;
    uint8_t percent_tempo;
} ysw_progression_t;

static inline ysw_slot_t *ysw_progression_get_slot(ysw_progression_t *progression, uint32_t index)
{
    return ysw_array_get(progression->slots, index);
}

static inline ysw_chord_t *ysw_progression_get_chord(ysw_progression_t *progression, uint32_t index)
{
    return ysw_progression_get_slot(progression, index)->chord;
}

static inline ysw_degree_t ysw_progression_get_degree(ysw_progression_t *progression, uint32_t index)
{
    return ysw_progression_get_slot(progression, index)->degree;
}

static inline uint32_t ysw_progression_get_chord_count(ysw_progression_t *progression)
{
    return ysw_array_get_count(progression->slots);
}

static inline uint32_t ysw_progression_get_chord_note_count(ysw_progression_t *progression, uint32_t index)
{
    return ysw_chord_get_note_count(ysw_progression_get_chord(progression, index));
}

static inline ysw_chord_note_t *ysw_slot_get_chord_note(ysw_slot_t *slot, uint32_t index)
{
    return ysw_array_get(slot->chord->chord_notes, index);
}

static inline uint32_t ysw_slot_get_chord_note_count(ysw_slot_t *slot)
{
    return ysw_array_get_count(slot->chord->chord_notes);
}

static inline uint32_t ysw_slot_get_duration(ysw_slot_t *slot)
{
    return slot->chord->duration;
}

note_t *ysw_progression_get_notes(ysw_progression_t *progression, uint8_t bpm);
uint32_t ysw_progression_get_note_count(ysw_progression_t *progression);
void ysw_progression_set_percent_tempo(ysw_progression_t *progression, uint8_t percent_tempo);
void ysw_progression_set_instrument(ysw_progression_t *progression, uint8_t instrument);
void ysw_progression_set_tonic(ysw_progression_t *progression, uint8_t tonic);
int ysw_progression_add_chord(ysw_progression_t *progression, uint8_t degree, ysw_chord_t *chord);
void ysw_progression_free(ysw_progression_t *progression);
ysw_progression_t *ysw_progression_create(char *name, uint8_t tonic, uint8_t instrument, uint8_t bpm);
void ysw_progression_dump(ysw_progression_t *progresssion, char *tag);
