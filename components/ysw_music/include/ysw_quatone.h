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

#define YSW_QUATONES_PER_DEGREE 2
#define YSW_QUATONES_PER_OCTAVE 14

typedef uint8_t ysw_quatone_t;

extern const char *ysw_quatone_offset_choices;
extern const char *ysw_quatone_octave_choices;
extern const char *ysw_quatone_offset_cardinal[];

uint8_t ysw_quatone_to_note(uint8_t scale_tonic, uint8_t root, ysw_quatone_t quatone);

static inline uint8_t ysw_quatone_octave_to_index(int8_t octave)
{
    return octave + 1;
}

static inline int8_t ysw_quatone_octave_from_index(uint8_t index)
{
    return index - 1;
}

static inline ysw_quatone_t ysw_quatone_create(int8_t octave, uint8_t offset)
{
    return ((octave + 1) * YSW_QUATONES_PER_OCTAVE) + offset;
}

static inline int8_t ysw_quatone_get_octave(ysw_quatone_t quatone)
{
    return (quatone / YSW_QUATONES_PER_OCTAVE) - 1;
}

static inline uint8_t ysw_quatone_get_offset(ysw_quatone_t quatone)
{
    return quatone % YSW_QUATONES_PER_OCTAVE;
}

static inline ysw_quatone_t ysw_quatone_set_octave(ysw_quatone_t quatone, int8_t new_octave)
{
    return ((new_octave + 1) * YSW_QUATONES_PER_OCTAVE) + ysw_quatone_get_offset(quatone);
}

static inline ysw_quatone_t ysw_quatone_set_offset(ysw_quatone_t quatone, uint8_t new_offset)
{
    return (quatone - ysw_quatone_get_offset(quatone)) + new_offset;
}
