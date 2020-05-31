// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#define YSW_MF_MAX_NAME_LENGTH 64

typedef enum {
    YSW_MF_CHORD_STYLE = 1,
    YSW_MF_STYLE_NOTE,
    YSW_MF_HARMONIC_PROGRESSION,
    YSW_MF_PROGRESSION_STEP,
    YSW_MF_MELODY,
    YSW_MF_RHYTHM,
    YSW_MF_MIX,
} ysw_mf_type_t;

