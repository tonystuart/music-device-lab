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

#define YSW_TICKS_DEFAULT_TPB 100 // ticks per beat
#define YSW_TICKS_DEFAULT_BPM 120 // beats per minutes

static inline uint32_t ysw_ticks_to_millis(uint32_t ticks, uint8_t bpm)
{
    return (ticks * 60000) / (bpm * YSW_TICKS_DEFAULT_TPB);
}

static inline uint32_t ysw_ticks_to_millis_by_tpb(uint32_t ticks, uint8_t bpm, uint32_t tpb)
{
    return (ticks * 60000) / (bpm * tpb);
}

