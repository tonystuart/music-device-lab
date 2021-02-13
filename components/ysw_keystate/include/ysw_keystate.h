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
#include "stdint.h"

typedef struct {
    uint32_t down_time;
    uint32_t repeat_count;
} ysw_keystate_state_t;

typedef struct {
    ysw_bus_t *bus;
    uint32_t size;
    ysw_keystate_state_t state[];
} ysw_keystate_t;

ysw_keystate_t *ysw_keystate_create(ysw_bus_t *bus, uint32_t size);
void ysw_keystate_on_press(ysw_keystate_t *keystate, uint8_t scan_code);
void ysw_keystate_on_release(ysw_keystate_t *keystate, uint8_t scan_code);
void ysw_keystate_free(ysw_keystate_t *keystate);
