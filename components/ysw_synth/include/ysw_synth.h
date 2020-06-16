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

typedef enum {
    YSW_SYNTH_NOTE_ON,
    YSW_SYNTH_NOTE_OFF,
    YSW_SYNTH_PROGRAM_CHANGE,
} ysw_synth_message_type_t;

typedef struct {
    uint8_t channel;
    uint8_t midi_note;
    uint8_t velocity;
} ysw_synth_note_on_t;

typedef struct {
    uint8_t channel;
    uint8_t midi_note;
} ysw_synth_note_off_t;

typedef struct {
    uint8_t channel;
    uint8_t program;
} ysw_synth_program_change_t;

typedef struct {
    ysw_synth_message_type_t type;
    union {
        ysw_synth_note_on_t note_on;
        ysw_synth_note_off_t note_off;
        ysw_synth_program_change_t program_change;
    };
} ysw_synth_message_t;

