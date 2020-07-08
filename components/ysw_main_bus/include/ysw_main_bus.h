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

// To avoid infinite loops, no event handler shall publish an event at the same or lower enum value.

typedef enum {
    YSW_MSG_PREV,
    YSW_MSG_PLAY,
    YSW_MSG_STOP,
    YSW_MSG_LOOP,
    YSW_MSG_NEXT,
    YSW_MSG_CLOSE,
    YSW_MSG_SETTINGS,
    YSW_MSG_NEW,
    YSW_MSG_SAVE,
    YSW_MSG_COPY,
    YSW_MSG_PASTE,
    YSW_MSG_TRASH,
    YSW_MSG_EDIT,
    YSW_MSG_SEL_STEP,
} ysw_msg_t;

void ysw_main_bus_create();
uint32_t ysw_main_bus_subscribe(void *cb, void *context);
uint32_t ysw_main_bus_publish(ysw_msg_t msg, void *details);
void ysw_main_bus_unsubscribe(uint32_t handle);
