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

typedef enum {
    YSW_ACCIDENTAL_FLAT = -1,
    YSW_ACCIDENTAL_NATURAL,
    YSW_ACCIDENTAL_SHARP,
} ysw_accidental_t;

int8_t ysw_degree_to_note(uint8_t scale_tonic, uint8_t root_number, int8_t degree_number, ysw_accidental_t accidental);
void ysw_degree_normalize(int8_t degree_number, uint8_t *remainder, int8_t *octave);
void ysw_degree_to_name(char *buffer, size_t buffer_size, int8_t degree_number, ysw_accidental_t accidental);
