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
#include "ysw_sn.h"
#include "ysw_degree.h"
#include "ysw_midi.h"
#include "ysw_note.h"
#include "ysw_ticks.h"
#include "ysw_time.h"

#define YSW_CS_DURATION (4 * YSW_TICKS_DEFAULT_TPQN)

#define YSW_CS_MUSIC_CHANNEL 0

#define CS_NAME_SZ 64

// See Miller (2nd edition), pp 38-42
// https://en.wikipedia.org/wiki/Mode_(music)

typedef enum PACKED {
    YSW_MODE_IONIAN,
    YSW_MODE_DORIAN,
    YSW_MODE_PHRYGIAN,
    YSW_MODE_LYDIAN,
    YSW_MODE_MIXOLYDIAN,
    YSW_MODE_AEOLIAN,
    YSW_MODE_LOCRIAN,
} ysw_mode_t;

// Chord Style (chord, strum, arpeggio, etc.)

typedef struct {
    char *name;
    ysw_array_t *sn_array;
    uint8_t instrument;
    uint8_t octave;
    ysw_mode_t mode;
    int8_t transposition;
    uint8_t tempo;
    uint8_t divisions;
} ysw_cs_t;

ysw_cs_t *ysw_cs_create(char *name, uint8_t instrument, uint8_t octave, ysw_mode_t mode, int8_t transposition, uint8_t tempo, ysw_time_t time);

ysw_cs_t *ysw_cs_copy(ysw_cs_t *old_cs);

static inline uint32_t ysw_cs_get_sn_count(ysw_cs_t *cs)
{
    return ysw_array_get_count(cs->sn_array);
}

static inline ysw_sn_t *ysw_cs_get_sn(ysw_cs_t *cs, uint32_t index)
{
    return ysw_array_get(cs->sn_array, index);
}

void ysw_cs_free(ysw_cs_t *cs);
uint32_t ysw_cs_add_sn(ysw_cs_t *cs, ysw_sn_t *sn);
void ysw_cs_sort_sn_array(ysw_cs_t *cs);
void ysw_cs_set_name(ysw_cs_t *cs, const char *name);
void ysw_cs_set_instrument(ysw_cs_t *cs, uint8_t instrument);
const char *ysw_cs_get_name(ysw_cs_t *cs);
uint8_t ysw_cs_get_instrument(ysw_cs_t *cs);
ysw_note_t *ysw_cs_get_notes(ysw_cs_t *cs, uint32_t *note_count);
ysw_note_t *ysw_cs_get_note(ysw_cs_t *cs, ysw_sn_t *sn);
void ysw_cs_dump(ysw_cs_t *cs, char *tag);
