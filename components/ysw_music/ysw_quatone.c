// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_quatone.h"
#include "ysw_degree.h"
#include "ysw_midi.h"
#include "esp_log.h"
#include "assert.h"
#include "stdio.h"

#define TAG "YSW_QUATONE"

// TODO: quatone cleanup
const char *ysw_quatone_choices =
"1st (octave lower)\n"
"1.5 (octave lower)\n"
"2nd (octave lower)\n"
"2.5 (octave lower)\n"
"3rd (octave lower)\n"
"3.5 (octave lower)\n"
"4th (octave lower)\n"
"4.5 (octave lower)\n"
"5th (octave lower)\n"
"5.5 (octave lower)\n"
"6th (octave lower)\n"
"6.5 (octave lower)\n"
"7th (octave lower)\n"
"7.5 (octave lower)\n"
"1st\n"
"1.5\n"
"2nd\n"
"2.5\n"
"3rd\n"
"3.5\n"
"4th\n"
"4.5\n"
"5th\n"
"5.5\n"
"6th\n"
"6.5\n"
"7th\n"
"7.5\n"
"1st (octave higher)\n"
"1.5 (octave higher)\n"
"2nd (octave higher)\n"
"2.5 (octave higher)\n"
"3rd (octave higher)\n"
"3.5 (octave higher)\n"
"4th (octave higher)\n"
"4.5 (octave higher)\n"
"5th (octave higher)\n"
"5.5 (octave higher)\n"
"6th (octave higher)\n"
"6.5 (octave higher)\n"
"7th (octave higher)\n"
"7.5 (octave higher)";

const char *ysw_quatone_cardinal[] = {
    "1st",
    "1.5",
    "2nd",
    "2.5",
    "3rd",
    "3.5",
    "4th",
    "4.5",
    "5th",
    "5.5",
    "6th",
    "6.5",
    "7th",
    "7.5",
};

// Degrees to Quatones

//     -1        +1
//    oct       oct
//
// 1 -  0   14   28
// 2 -  2   16   30
// 3 -  4   18   32
// 4 -  6   20   34 (includes virtual black key)
// 5 -  8   22   36
// 6 - 10   24   38
// 7 - 12   26   40

// 8 - 14 28 42 (includes virtual black key)

// +-----+---+---+---+-----+-----+---+---+---+---+---+-----+-----+---+---+---+-----+-----+---+---+---+---+---+-----+
// |     | 1 |   | 3 |     |     | 6 |   | 8 |   |10 |     |     |13 |   |15 |     |     |18 |   |20 |   |22 |     |
// |     |   |   |   |     |     |   |   |   |   |   |     |     |   |   |   |     |     |   |   |   |   |   |     |
// |     |   |   |   |     |     |   |   |   |   |   |     |     |   |   |   |     |     |   |   |   |   |   |     |
// |     |   |   |   |     |     |   |   |   |   |   |     |     |   |   |   |     |     |   |   |   |   |   |     |
// |     +---+   +---+     |     +---+   +---+   +---+     |     +---+   +---+     |     +---+   +---+   +---+     |
// |   C   |   D   |   E   |   F   |   G   |   A   |  B    |       |       |       |       |       |       |       |
// |       |       |       |       |       |       |       |       |       |       |       |       |       |       |
// |   0   |   2   |   4   |   5   |   7   |   9   |  11   |  12   |  14   |  16   |  17   |  19   |  21   |  23   |
// +-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+-------+
// C...0   1   2   3   4   5   5   6   7   8   9  10  11  12
//         0   1   2   3   4   4   5   6   7   8   9  10  11  11
// D...........0   1   2   3   3   4   5   6   7   8   9  10  10  11
//                 0   1   2   2   3   4   5   6   7   8   9   9  10  11
// E...................0   1   1   2   3   4   5   6   7   8   8   9  10  11
//                         *   *   *   *   *   *   *   *   *   *   *   *   *   *
// F...........................0   1   2   3   4   5   6   7   7   8   9  10  11  12
//                                 0   1   2   3   4   5   6   6   7   8   9  10  11  11
// G...................................0   1   2   3   4   5   5   6   7   8   9  10  10  11
//                                         0   1   2   3   4   4   5   6   7   8   9   9  10  11
// A...........................................0   1   2   3   3   4   5   6   7   8   8   9  10  11
//                                                 0   1   2   2   3   4   5   6   7   7   8   9  10  11
// B...................................................0   1   1   2   3   4   5   6   6   7   8   9  10  11
//                                                         *   *   *   *   *   *   *   *   *   *   *   *   *   *

// 1. Each tone is a semitone higher than the previous tone
// 2. The quatone for the two missing black keys in each octave is enharmonic with respect to the next tone
// 3. There are 42 quatones that range in value from 0 to 41:
//        0 to 13 are -1 octave
//       14 to 27 are the standard middle octave
//       28 to 41 are +1 octave
// 4. To use the ysw_quatone_interval table, we calculate:
//       octave = (value / 14) - 1
//       quatone = value % 14

const uint8_t ysw_quatone_intervals[7][14] = {
    /* C */ { 0, 1, 2, 3, 4, 5, 5, 6, 7, 8, 9, 10, 11, 12 },
    /* D */ { 0, 1, 2, 3, 3, 4, 5, 6, 7, 8, 9, 10, 10, 11 },
    /* E */ { 0, 1, 1, 2, 3, 4, 5, 6, 7, 8, 8, 9, 10, 11 },
    /* F */ { 0, 1, 2, 3, 4, 5, 6, 7, 7, 8, 9, 10, 11, 12 },
    /* G */ { 0, 1, 2, 3, 4, 5, 5, 6, 7, 8, 9, 10, 10, 11 },
    /* A */ { 0, 1, 2, 3, 3, 4, 5, 6, 7, 8, 8, 9, 10, 11 },
    /* B */ { 0, 1, 1, 2, 3, 4, 5, 6, 6, 7, 8, 9, 10, 11 },
};

// TODO: quatone cleanup
uint8_t ysw_quatone_to_note(uint8_t scale_tonic, uint8_t root, ysw_quatone_t quatone)
{
    int8_t root_octave = root / 7;
    int8_t root_offset = root % 7;

    int8_t quatone_octave = ysw_quatone_get_octave(quatone);
    int8_t quatone_offset = ysw_quatone_get_offset(quatone);

    int8_t root_interval = ysw_degree_intervals[0][root_offset];
    int8_t quatone_interval = ysw_quatone_intervals[root_offset][quatone_offset];

    uint8_t note = scale_tonic +
        ((12 * root_octave) + root_interval) +
        ((12 * quatone_octave) + quatone_interval);

    return note;
}

