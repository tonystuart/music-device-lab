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
#include "driver/i2c.h"
#include "stdint.h"

void ysw_touch_create_task(ysw_bus_t *bus, i2c_port_t port, gpio_num_t sda, gpio_num_t scl, ysw_array_t *addresses);
