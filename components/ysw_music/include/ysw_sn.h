// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_accidental.h"
#include "ysw_array.h"
#include "ysw_degree.h"
#include "ysw_note.h"

#define YSW_CSN_MIN_DURATION 10
#define YSW_CSN_MIN_DEGREE (-21)
#define YSW_CSN_MAX_DEGREE (+21)
#define YSW_CSN_MAX_VELOCITY 120
#define YSW_CSN_TICK_INCREMENT 5

// Chord Style Note state fields

#define YSW_SN_SELECTED 0b00000001

// Chord Style Note (a note in a chord style)

typedef struct {
    uint32_t start;
    uint32_t duration;
    int8_t degree;
    uint8_t velocity;
    uint8_t flags; // persistent
    uint8_t state; // transient
} ysw_sn_t;

// TODO: factor out generic parts and move to ysw_accidental.h

static inline bool ysw_sn_is_natural(const ysw_sn_t *sn)
{
    return (sn->flags & YSW_ACCIDENTAL_MASK) == YSW_ACCIDENTAL_NATURAL_BIT;
}

static inline bool ysw_sn_is_flat(const ysw_sn_t *sn)
{
    return (sn->flags & YSW_ACCIDENTAL_MASK) == YSW_ACCIDENTAL_FLAT_BIT;
}

static inline bool ysw_sn_is_sharp(const ysw_sn_t *sn)
{
    return (sn->flags & YSW_ACCIDENTAL_MASK) == YSW_ACCIDENTAL_SHARP_BIT;
}

static inline void ysw_sn_set_natural(ysw_sn_t *sn)
{
    sn->flags &= ~YSW_ACCIDENTAL_MASK;
}

static inline void ysw_sn_set_flat(ysw_sn_t *sn)
{
    sn->flags = (sn->flags & ~YSW_ACCIDENTAL_MASK) | YSW_ACCIDENTAL_FLAT_BIT;
}

static inline void ysw_sn_set_sharp(ysw_sn_t *sn)
{
    sn->flags = (sn->flags & ~YSW_ACCIDENTAL_MASK) | YSW_ACCIDENTAL_SHARP_BIT;
}

static inline ysw_accidental_t ysw_sn_get_accidental(const ysw_sn_t *sn)
{
    if (ysw_sn_is_natural(sn)) {
        return YSW_ACCIDENTAL_NATURAL;
    }
    if (ysw_sn_is_flat(sn)) {
        return YSW_ACCIDENTAL_FLAT;
    }
    return YSW_ACCIDENTAL_SHARP;
}

static inline void ysw_sn_set_accidental(ysw_sn_t *sn, ysw_accidental_t accidental)
{
    sn->flags = (sn->flags & ~YSW_ACCIDENTAL_MASK) | ysw_accidental_to_bit_mask(accidental);
}

static inline void ysw_sn_select(ysw_sn_t *sn, bool selected)
{
    if (selected) {
        sn->state |= YSW_SN_SELECTED;
    } else {
        sn->state &= ~YSW_SN_SELECTED;
    }
}

static inline bool ysw_sn_is_selected(ysw_sn_t *sn)
{
    return sn->state & YSW_SN_SELECTED;
}

ysw_sn_t *ysw_sn_create(int8_t degree, uint8_t velocity, uint32_t start, uint32_t duration, uint8_t flags);
ysw_sn_t *ysw_sn_copy(ysw_sn_t *sn);
void ysw_sn_free(ysw_sn_t *ysw_sn);
void ysw_sn_normalize(ysw_sn_t *sn);
uint8_t ysw_sn_to_midi_note(ysw_sn_t *sn, uint8_t scale_tonic, uint8_t root_number);
int ysw_sn_compare(const void *left, const void *right);
