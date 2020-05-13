// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "stdint.h"
#include "stddef.h"

extern const uint8_t ysw_degree_intervals[7][7];

typedef enum {
    I = 1,
    II,
    III,
    IV,
    V,
    VI,
    VII,
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

typedef enum {
    YSW_ACCIDENTAL_FLAT = -1,
    YSW_ACCIDENTAL_NATURAL,
    YSW_ACCIDENTAL_SHARP,
} ysw_accidental_t;

uint8_t ysw_degree_to_note(uint8_t scale_tonic, uint8_t root_number, int8_t degree_number, ysw_accidental_t accidental);
void ysw_degree_normalize(int8_t degree_number, uint8_t *normalized_degree_number, int8_t *octave);
const char* ysw_degree_get_name(uint8_t name_index);
