// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "stdint.h"

// See https://en.wikipedia.org/wiki/Tempo

// Although the note value of a beat should typically be that indicated
// by the denominator of the time signature, we use beats per quarter note
// regardless of the denominator of the time signature. 

const char *ysw_tempo =
"10 BPM\n"
"20 BPM\n"
"30 BPM\n"
"40 BPM\n"
"50 BPM\n"
"60 BPM\n"
"70 BPM\n"
"80 BPM\n"
"90 BPM\n"
"100 BPM\n"
"110 BPM\n"
"120 BPM\n"
"130 BPM\n"
"140 BPM\n"
"150 BPM\n"
"160 BPM\n"
"170 BPM\n"
"180 BPM";

uint8_t ysw_tempo_from_index(uint8_t index)
{
    return (index + 1) * 10;
}

int8_t ysw_tempo_to_index(int8_t tempo)
{
    return (tempo / 10) - 1;
}
