// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "assert.h"
#include "stdint.h"

const char *ysw_division =
"1 (400 TPD)\n"
"2 (200 TPD)\n"
"3 (133 TPD)\n"
"4 (100 TPD)\n"
"6 (66 TPD)\n"
"8 (50 TPD)\n"
"16 (25 TPD)";

const uint8_t index_division_map[] = {
    1, 2, 3, 4, 6, 8, 16
};

#define INDEX_DIVISION_MAP_SZ (sizeof(index_division_map) / sizeof(const uint8_t))

const char *ysw_division_ticks[] = {
    "400", // 400 / 1 = 400 / 400 = 1.00 (whole note)
    "200", // 400 / 2 = 200 / 400 = 0.50 (half note)
    "133", // 400 / 3 = 133 / 400 = 0.33 (third note - wtf!)
    "100", // 400 / 4 = 100 / 400 = 0.25 (quarter note)
    "66",  // 400 / 6 =  66 / 400 = 0.16 (sixth note - wtf!)
    "50",  // 400 / 8 =  50 / 400 = 0.125 (eight note)
    "25",  // 400 / 16 = 25 / 400 = 0.0625 (sixteenth note)
};

uint8_t ysw_division_from_index(uint8_t index)
{
    assert(index < INDEX_DIVISION_MAP_SZ);
    return index_division_map[index];
}

uint8_t ysw_division_to_index(int8_t divisions)
{
    int8_t index = -1;
    for (uint8_t i = 0; i < INDEX_DIVISION_MAP_SZ && index < 0; i++) {
        if (divisions == index_division_map[i]) {
            index = i;
        }
    }
    assert(index >= 0);
    return index;
}

const char *ysw_division_to_tick(uint8_t divisions)
{
    uint8_t index = ysw_division_to_index(divisions);
    return ysw_division_ticks[index];
}

