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

void seq_initialize();
void seq_send(ysw_seq_message_t *message);
void seq_rendezvous(ysw_seq_message_t *message);

