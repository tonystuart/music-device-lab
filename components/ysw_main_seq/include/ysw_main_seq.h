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
#include "lvgl.h"

void ysw_main_seq_initialize();
void ysw_main_seq_send(ysw_seq_message_t *message);
void ysw_main_seq_rendezvous(ysw_seq_message_t *message);
void ysw_main_seq_init_loop_btn(lv_obj_t *btn);
void ysw_main_seq_on_stop(void *context, lv_obj_t *btn);
void ysw_main_seq_on_loop(void *context, lv_obj_t *btn);
