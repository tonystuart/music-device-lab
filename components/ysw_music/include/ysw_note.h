// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_common.h"
#include "stdint.h"

typedef struct PACKED note {
    uint32_t start;
    uint16_t duration;
    uint8_t channel;
    uint8_t midi_note;
    uint8_t velocity;
    uint8_t instrument;
} ysw_note_t;

