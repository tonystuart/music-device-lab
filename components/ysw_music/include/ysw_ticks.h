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

#define YSW_TICKS_DEFAULT_TPQN 100 // ticks per quarter note
#define YSW_TICKS_DEFAULT_BPQN 120 // beats per quarter note

static inline uint32_t ysw_ticks_to_millis(uint32_t ticks, uint8_t qnpm)
{
    return (ticks * 60000) / (qnpm * YSW_TICKS_DEFAULT_TPQN);
}

static inline uint32_t ysw_ticks_to_millis_by_tpqn(uint32_t ticks, uint8_t qnpm, uint32_t tpqn)
{
    return (ticks * 60000) / (qnpm * tpqn);
}

