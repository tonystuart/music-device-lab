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

typedef enum PACKED {
    /* 0 */ YSW_TIME_1_16 = 0,
    /* 1 */ YSW_TIME_1_8,
    /* 2 */ YSW_TIME_1_4,
    /* 3 */ YSW_TIME_2_2,
    /* 4 */ YSW_TIME_2_4,
    /* 5 */ YSW_TIME_3_4,
    /* 6 */ YSW_TIME_4_4,
    /* 7 */ YSW_TIME_6_8,
    /* 8 */ YSW_TIME_8_8,
} ysw_time_t;

extern const char *ysw_time;

uint8_t ysw_time_to_beats_per_measure(ysw_time_t time);
uint8_t ysw_time_to_beat_unit(ysw_time_t time);
uint16_t ysw_time_to_measure_duration(ysw_time_t time);
