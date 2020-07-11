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

// See https://en.wikipedia.org/wiki/Accidental_(music)

// In music, an accidental is a note of a pitch (or pitch class) that
// is not a member of the scale or mode indicated by the most recently
// applied key signature. In musical notation, the sharp (♯), flat (♭),
// and natural (♮) symbols, among others, mark such notes—and those symbols
// are also called accidentals.

typedef enum {
    YSW_ACCIDENTAL_FLAT = -1,
    YSW_ACCIDENTAL_NATURAL,
    YSW_ACCIDENTAL_SHARP,
} ysw_accidental_t;

#define YSW_ACCIDENTAL_MASK 0b11
#define YSW_ACCIDENTAL_NATURAL_BIT 0b00
#define YSW_ACCIDENTAL_FLAT_BIT    0b01
#define YSW_ACCIDENTAL_SHARP_BIT   0b10

extern const char *ysw_accidental;

static inline uint8_t ysw_accidental_from_index(uint16_t index)
{
    return index - 1;
}

static inline uint8_t ysw_accidental_to_index(uint16_t accidental)
{
    return accidental + 1;
}

static inline uint8_t ysw_accidental_to_bit_mask(ysw_accidental_t accidental)
{
    uint8_t bit_mask = 0;
    switch (accidental) {
        case YSW_ACCIDENTAL_FLAT:
            bit_mask = YSW_ACCIDENTAL_FLAT_BIT;
            break;
        case YSW_ACCIDENTAL_NATURAL:
            bit_mask = YSW_ACCIDENTAL_NATURAL_BIT;
            break;
        case YSW_ACCIDENTAL_SHARP:
            bit_mask = YSW_ACCIDENTAL_SHARP_BIT;
            break;
    }
    return bit_mask;
}
