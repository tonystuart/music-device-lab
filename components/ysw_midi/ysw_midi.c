// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_midi.h"

static bool diatonic_midi_notes[] = {
    true, false, true, false, true, true, false, true, false, true, false, true,
};

bool ysw_midi_is_diatonic(uint8_t midi_note)
{
    return diatonic_midi_notes[midi_note % 12];
}


