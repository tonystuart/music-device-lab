// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_time.h"

#include "assert.h"
#include "stdint.h"

// See https://en.wikipedia.org/wiki/Time_signature

const char *ysw_time =
"2/2\n"
"2/4\n"
"3/4\n"
"4/4\n"
"6/8\n"
;


static const uint8_t ysw_time_beats_per_measure[] = {
    2,
    2,
    3,
    4,
    6,
};

#define YSW_TIME_BEATS_PER_MEASURE_SZ (sizeof(ysw_time_beats_per_measure) / sizeof(uint8_t))

static const uint8_t ysw_time_beat_unit[] = {
    2,
    4,
    4,
    4,
    8,
};

#define YSW_TIME_BEAT_UNIT_SZ (sizeof(ysw_time_beat_unit) / sizeof(uint8_t))

uint8_t ysw_time_to_beats_per_measure(ysw_time_t time)
{
    assert(time < YSW_TIME_BEATS_PER_MEASURE_SZ);
    return ysw_time_beats_per_measure[time];
}

uint8_t ysw_time_to_beat_unit(ysw_time_t time)
{
    assert(time < YSW_TIME_BEAT_UNIT_SZ);
    return ysw_time_beat_unit[time];
}

