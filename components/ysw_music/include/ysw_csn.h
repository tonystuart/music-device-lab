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
#include "ysw_degree.h"

#define YSW_CSN_MIN_DURATION 10
#define YSW_CSN_MIN_DEGREE (-21)
#define YSW_CSN_MAX_DEGREE (+21)
#define YSW_CSN_MAX_VELOCITY 120
#define YSW_CSN_TICK_INCREMENT 5

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

#define YSW_CSN_ACCIDENTAL 0b11
#define YSW_CSN_NATURAL 0b00
#define YSW_CSN_FLAT    0b01
#define YSW_CSN_SHARP   0b10

// Chord Style Note state fields

#define YSW_CSN_SELECTED 0b00000001

// Chord Style Note (a note in a chord style)

typedef struct {
    uint32_t start;
    uint32_t duration;
    int8_t degree;
    uint8_t velocity;
    uint8_t flags; // persistent
    uint8_t state; // transient
} ysw_csn_t;

static inline bool ysw_csn_is_natural(const ysw_csn_t *csn)
{
    return (csn->flags & YSW_CSN_ACCIDENTAL) == YSW_CSN_NATURAL;
}

static inline bool ysw_csn_is_flat(const ysw_csn_t *csn)
{
    return (csn->flags & YSW_CSN_ACCIDENTAL) == YSW_CSN_FLAT;
}

static inline bool ysw_csn_is_sharp(const ysw_csn_t *csn)
{
    return (csn->flags & YSW_CSN_ACCIDENTAL) == YSW_CSN_SHARP;
}

static inline void ysw_csn_set_natural(ysw_csn_t *csn)
{
    csn->flags &= ~YSW_CSN_ACCIDENTAL;
}

static inline void ysw_csn_set_flat(ysw_csn_t *csn)
{
    csn->flags = (csn->flags & ~YSW_CSN_ACCIDENTAL) | YSW_CSN_FLAT;
}

static inline void ysw_csn_set_sharp(ysw_csn_t *csn)
{
    csn->flags = (csn->flags & ~YSW_CSN_ACCIDENTAL) | YSW_CSN_SHARP;
}

static inline ysw_accidental_t ysw_csn_get_accidental(const ysw_csn_t *csn)
{
    if (ysw_csn_is_natural(csn)) {
        return YSW_ACCIDENTAL_NATURAL;
    }
    if (ysw_csn_is_flat(csn)) {
        return YSW_ACCIDENTAL_FLAT;
    }
    return YSW_ACCIDENTAL_SHARP;
}

static inline void ysw_csn_select(ysw_csn_t *csn, bool selected)
{
    if (selected) {
        csn->state |= YSW_CSN_SELECTED;
    } else {
        csn->state &= ~YSW_CSN_SELECTED;
    }
}

static inline bool ysw_csn_is_selected(ysw_csn_t *csn)
{
    return csn->state & YSW_CSN_SELECTED;
}

void ysw_csn_free(ysw_csn_t *ysw_csn);
ysw_csn_t *ysw_csn_create(int8_t degree, uint8_t velocity, uint32_t start, uint32_t duration, uint8_t flags);
ysw_csn_t *ysw_csn_copy(ysw_csn_t *csn);
uint8_t ysw_csn_to_midi_note(ysw_csn_t *csn, uint8_t scale_tonic, uint8_t root_number);
int ysw_csn_compare(const void *left, const void *right);
