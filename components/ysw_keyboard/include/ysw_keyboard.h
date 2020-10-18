// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_array.h"
#include "ysw_bus.h"

typedef struct {
    ysw_array_t *rows;
    ysw_array_t *columns;
} ysw_keyboard_config_t;

void ysw_keyboard_create_task(ysw_bus_h bus, ysw_keyboard_config_t *keyboard_config);
