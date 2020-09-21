// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_system.h"
#include "math.h"
#include "time.h"

uint64_t ysw_system_get_time_in_millis()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t millis = ts.tv_sec * 1000 + lround(ts.tv_nsec / 1000000);
    return millis;
}

uint32_t ysw_system_get_reference_time_in_millis()
{
    uint64_t current_millis = ysw_system_get_time_in_millis();
    static uint64_t base_millis;
    if (!base_millis) {
        base_millis = current_millis;
    }
    return current_millis - base_millis;
}

