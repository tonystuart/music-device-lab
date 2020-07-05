// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_bus.h"


typedef enum {
    PLAY_STEP,
} ysw_msg_t;

void ysw_main_bus_create();
uint32_t ysw_main_bus_subscribe(void *context, void *cb);
uint32_t ysw_main_bus_publish(ysw_msg_t msg, void *details);
void ysw_main_bus_unsubscribe(uint32_t handle);
