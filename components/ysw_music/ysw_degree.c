// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_degree.h"

#include "ysw_midi.h"

#include "esp_log.h"
#include "assert.h"
#include "stdio.h"

#define TAG "YSW_DEGREE"

// See https://en.wikipedia.org/wiki/Degree_(music)

// "In set theory, for instance, the 12 degrees of the chromatic scale usually are
// numbered starting from C=0, the twelve pitch classes being numbered from 0 to 11."

// See https://en.wikipedia.org/wiki/Roman_numeral_analysis

const char *ysw_degree_choices =
"I\n"
"II\n"
"III\n"
"IV\n"
"V\n"
"VI\n"
"VII";

const uint8_t ysw_degree_intervals[7][7] = {
    /* C */ { 0, 2, 4, 5, 7, 9, 11 },
    /* D */ { 0, 2, 3, 5, 7, 9, 10 },
    /* E */ { 0, 1, 3, 5, 7, 8, 10 },
    /* F */ { 0, 2, 4, 6, 7, 9, 11 },
    /* G */ { 0, 2, 4, 5, 7, 9, 10 },
    /* A */ { 0, 2, 3, 5, 7, 8, 10 },
    /* B */ { 0, 1, 3, 5, 6, 8, 10 },
};

