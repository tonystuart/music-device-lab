// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_sequencer.h"

typedef void (*csc_metronome_cb_t)(int32_t tick);

void sequencer_send(ysw_sequencer_message_t *message);
void sequencer_initialize(csc_metronome_cb_t csc_metronome_cb);

