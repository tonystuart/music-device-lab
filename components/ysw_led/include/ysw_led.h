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

typedef struct {
    uint8_t gpio;
} ysw_led_config_t;

void ysw_led_create_task(ysw_bus_t *bus, ysw_led_config_t *config);
