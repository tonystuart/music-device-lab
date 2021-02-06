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
#include "ysw_i2c.h"

void ysw_touch_create_task(ysw_bus_t *bus, ysw_i2c_t *i2c, ysw_array_t *addresses);
