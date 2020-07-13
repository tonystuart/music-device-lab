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
#include "ysw_degree.h"
#include "ysw_note.h"
#include "ysw_quatone.h"

#define YSW_CSN_MIN_DURATION 10
#define YSW_CSN_MIN_DEGREE (-21)
#define YSW_QUATONE_MAX (41)
#define YSW_CSN_MAX_VELOCITY 120
#define YSW_CSN_TICK_INCREMENT 5

// Chord Style Note state fields

#define YSW_SN_SELECTED 0b00000001

// Chord Style Note (a note in a chord style)

typedef struct {
    uint32_t start;
    uint32_t duration;
    uint8_t quatone;
    uint8_t velocity;
    uint8_t flags; // persistent
    uint8_t state; // transient
} ysw_sn_t;

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

ysw_sn_t *ysw_sn_create(ysw_quatone_t quatone, uint8_t velocity, uint32_t start, uint32_t duration, uint8_t flags);
ysw_sn_t *ysw_sn_copy(ysw_sn_t *sn);
void ysw_sn_free(ysw_sn_t *ysw_sn);
void ysw_sn_normalize(ysw_sn_t *sn);
uint8_t ysw_sn_to_midi_note(ysw_sn_t *sn, uint8_t scale_tonic, uint8_t root_number);
int ysw_sn_compare(const void *left, const void *right);
