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
"1/16 (CS)\n" // 0
"1/8 (CS)\n"  // 1
"1/4 (CS)\n"  // 2
"2/2\n"       // 3
"2/4\n"       // 4
"3/4\n"       // 5
"4/4\n"       // 6
"6/8\n"       // 7
"8/8 (CS)";   // 8

// Time Signature - Beats per Measure (Numerator)

static const uint8_t ysw_time_beats_per_measure[] = {
    1,
    1,
    1,
    2,
    2,
    3,
    4,
    6,
    8,
};

#define YSW_TIME_BEATS_PER_MEASURE_SZ (sizeof(ysw_time_beats_per_measure) / sizeof(uint8_t))

// Time Signature - Beat Unit (Denominator)

static const uint8_t ysw_time_beat_unit[] = {
    16,
    8,
    4,
    2,
    4,
    4,
    4,
    8,
    8,
};

#define YSW_TIME_BEAT_UNIT_SZ (sizeof(ysw_time_beat_unit) / sizeof(uint8_t))

// YSW_TICKS_DEFAULT_TPQN defines the number of ticks per quarter note (currently 100)

// The duration (in ticks) of a measure depends on the ratio of the
// quarter note beat unit to the time signature beat unit.

// This gives us the following formula for finding measure duration:
// duration_in_ticks = beats_per_measure * (4 / beat unit) * ticks_per_beat

static const uint16_t ysw_time_measure_duration[] = {
    25,  // 1/16 = 1 * (4/16) * 100 = 25
    50,  // 1/8 = 1 * (4/8) * 100 = 50
    100, // 1/4 = 1 * (4/4) * 100 = 100
    400, // 2/2 = 2 * (4/2) * 100 = 400
    200, // 2/4 = 2 * (4/4) * 100 = 200
    300, // 3/4 = 3 * (4/4) * 100 = 300
    400, // 4/4 = 4 * (4/4) * 100 = 400
    300, // 6/8 = 6 * (4/8) * 100 = 300
    400, // 8/8 = 8 * (4/8) * 100 = 400
};

#define YSW_TIME_MEASURE_DURATION_SZ (sizeof(ysw_time_measure_duration) / sizeof(uint16_t))

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

uint16_t ysw_time_to_measure_duration(ysw_time_t time)
{
    assert(time < YSW_TIME_MEASURE_DURATION_SZ);
    return ysw_time_measure_duration[time];
}
