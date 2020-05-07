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
    YSW_TIME_2_2 = 0,
    YSW_TIME_2_4,
    YSW_TIME_3_4,
    YSW_TIME_4_4,
    YSW_TIME_6_8,
} ysw_time_t;

extern const char *ysw_time;

uint8_t ysw_time_to_beats_per_measure(ysw_time_t time);
uint8_t ysw_time_to_beat_unit(ysw_time_t time);
