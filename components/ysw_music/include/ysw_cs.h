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
#include "ysw_csn.h"
#include "ysw_degree.h"

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
    ysw_array_t *csns;
    uint32_t duration;
    uint8_t instrument;
    uint8_t octave;
    ysw_mode_t mode;
    int8_t transposition;
} ysw_cs_t;

static inline uint32_t ysw_cs_get_csn_count(ysw_cs_t *cs)
{
    return ysw_array_get_count(cs->csns);
}

static inline ysw_csn_t *ysw_cs_get_csn(ysw_cs_t *cs, uint32_t index)
{
    return ysw_array_get(cs->csns, index);
}

ysw_cs_t *ysw_cs_create(char *name, uint32_t duration, uint8_t instrument, uint8_t octave, ysw_mode_t mode, int8_t transposition);

void ysw_cs_free(ysw_cs_t *cs);

uint32_t ysw_cs_add_csn(ysw_cs_t *cs, ysw_csn_t *csn);

void ysw_cs_sort_csns(ysw_cs_t *cs);

void ysw_cs_set_duration(ysw_cs_t *cs, uint32_t duration);

void ysw_cs_set_name(ysw_cs_t *cs, const char *name);

void ysw_cs_set_instrument(ysw_cs_t *cs, uint8_t instrument);

uint32_t ysw_cs_get_duration(ysw_cs_t *cs);

const char *ysw_cs_get_name(ysw_cs_t *cs);

uint8_t ysw_cs_get_instrument(ysw_cs_t *cs);

note_t *ysw_cs_get_notes(ysw_cs_t *cs, uint32_t *note_count);

void ysw_cs_dump(ysw_cs_t *cs, char *tag);
