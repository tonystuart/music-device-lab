// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_ticks.h"

#define YSW_TICKS_PER_QUARTER_NOTE 256 // should use ZM_QUARTER, but that creates a circular dependency

// We use the term Beats per Minute (BPM) for Quarter Notes per Minute, regardless of time signature

// Here is an example of converting from millis to ticks:

// Ticks per Quarter Note (tpqn) = 256
// Beats per Minute (bpm) = 120

// 120 beats per minute, 2 beats per second (a second per two beats),
// take inverse (i.e. 1/x) result is 1 beat per half second (500 ms).

// 256 ticks      quarter note
// ------------ * ------------ (read as per)
// quarter note   500 millis

// 256 ticks / 500 millis = 0.512 tick per milli

// millis * ticks_per_milli = ticks
// which is ticks = millis * (tpqn / ((1 / (bpm / 60)) * 1000))

// Let's work some examples.

// 1. 2000 millis at 120 beats per minute should be 1024 ticks (a whole note)
// millis * (tpqn / ((1 / (bpm / 60)) * 1000))
// 2000 * (256 / ((1 / (120 / 60)) * 1000))
// 2000 * (256 / ((1 / 2) * 1000))
// 2000 * (256 / (.5 * 1000))
// 2000 * (256 / 500)
// 2000 * .512
// 1024

// 2. 1000 millis at 60 beats per minute should be 256 ticks (a quarter note)
// millis * (tpqn / ((1 / (bpm / 60)) * 1000))
// 1000 * (256 / ((1 / (60 / 60)) * 1000))
// 1000 * (256 / ((1 / 1) * 1000))
// 1000 * (256 / (1 * 1000))
// 1000 * (256 / 1000)
// 1000 * .256
// 256

// Now rearrange terms to minimize integer divides losses

// millis * (tpqn / ((1 / (bpm / 60)) * 1000)) // original formula
// millis * (tpqn / (60 / bpm) * 1000))        // invert reciprocal
// millis * (tpqn / (60000 / bpm))             // distribute factor

// millis * tpqn   60000
// ------------- / -----                        // rewrite as step of fractions
// 1               bpm

// millis * tpqn   bpm
// ------------- * -----                        // invert and multiply
// 1               60000

// Yielding a simplified formula that minimizes losses due to integer divide

// ticks = (millis * tpqn * bpm) / 60000

static inline uint32_t ysw_millis_to_ticks_by_tpqn(uint32_t millis, uint8_t bpm, uint32_t tpqn)
{
    return (millis * tpqn * bpm) / 60000;
}

static inline uint32_t ysw_ticks_to_millis_by_tpqn(uint32_t ticks, uint8_t bpm, uint32_t tpqn)
{
    return (ticks * 60000) / (bpm * tpqn);
}

static inline uint32_t ysw_millis_to_ticks(uint32_t millis, uint8_t bpm)
{
    return ysw_millis_to_ticks_by_tpqn(millis, bpm, YSW_TICKS_PER_QUARTER_NOTE);
}

static inline uint32_t ysw_ticks_to_millis(uint32_t ticks, uint8_t bpm)
{
    return ysw_ticks_to_millis_by_tpqn(ticks, bpm, YSW_TICKS_PER_QUARTER_NOTE);
}
