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

#define YSW_QUATONES_PER_OCTAVE 14

typedef uint8_t ysw_quatone_t;

extern const char *ysw_quatone_choices;
extern const char *ysw_quatone_cardinal[];

uint8_t ysw_quatone_to_note(uint8_t scale_tonic, uint8_t root, ysw_quatone_t quatone);

static inline int8_t ysw_quatone_get_octave(ysw_quatone_t quatone)
{
    return (quatone / YSW_QUATONES_PER_OCTAVE) - 1;
}

static inline uint8_t ysw_quatone_get_offset(ysw_quatone_t quatone)
{
    return quatone % YSW_QUATONES_PER_OCTAVE;
}
