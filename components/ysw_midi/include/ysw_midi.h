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

#define MAX_NOTES 128
#define SEMITONES_PER_OCTAVE 12
#define DRUM_CHANNEL 9

#define LOWEST 24
#define HIGHEST 96
#define MIDDLE 60

bool ysw_midi_is_diatonic(uint8_t midi_note);
