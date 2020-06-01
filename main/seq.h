// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_seq.h"

typedef enum {
    META_NOTE,
    NOT_PLAYING,
} seq_cb_type_t;

typedef struct {
    seq_cb_type_t type;
    union {
        ysw_note_t *meta_note;
    };
} seq_cb_message_t;

typedef void (*seq_cb_t)(seq_cb_message_t *seq_cb_message);

void seq_send(ysw_seq_message_t *message);
void seq_initialize(seq_cb_t seq_cb);

