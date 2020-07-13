// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_common.h"
#include "stdint.h"
#include "stddef.h"

extern const char *ysw_degree_choices;
extern const uint8_t ysw_degree_intervals[7][7];

typedef enum {
    I,
    II,
    III,
    IV,
    V,
    VI,
    VII,
} ysw_degree_t;

typedef enum {
    TONIC,
    SUPERTONIC,
    MEDIANT,
    SUBDOMINANT,
    DOMINANT,
    SUBMEDIANT,
    SUBTONIC,
} ysw_role_t;

