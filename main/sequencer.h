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

typedef enum {
    META_NOTE,
    NOT_PLAYING,
} sequencer_cb_type_t;

typedef struct {
    sequencer_cb_type_t type;
    union {
        note_t *meta_note;
    };
} sequencer_cb_message_t;

typedef void (*sequencer_cb_t)(sequencer_cb_message_t *sequencer_cb_message);

void sequencer_send(ysw_sequencer_message_t *message);
void sequencer_initialize(sequencer_cb_t sequencer_cb);

