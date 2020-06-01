// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "stdbool.h"
#include "stdint.h"

#define YSW_MIDI_MAX 128

#define YSW_MIDI_DRUM_CHANNEL 9
#define YSW_MIDI_MAX_CHANNELS 16

#define YSW_MIDI_STATUS_NOTE 0
#define YSW_MIDI_STATUS_CHANNEL (YSW_MIDI_MAX_CHANNELS - 1)

#define YSW_MIDI_UNPO 7 // 7 Unique Notes per Octave
#define YSW_MIDI_USPO 12 // 12 Unique Semitones per Octave

// https://newt.phys.unsw.edu.au/jw/notes.html

#define YSW_MIDI_LPN 21 // Lowest Piano Note
#define YSW_MIDI_HPN 108 // Highest Piano Note
#define YSW_MIDI_MIDDLE_C 60

bool ysw_midi_is_diatonic(uint8_t midi_note);

static inline uint8_t ysw_midi_get_mode(uint8_t scale_root)
{
    return scale_root % YSW_MIDI_USPO;
}

static inline uint8_t ysw_midi_get_octave(uint8_t midi_note)
{
    return midi_note / YSW_MIDI_USPO;
}
