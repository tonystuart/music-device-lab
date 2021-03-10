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
#include "stdlib.h"

typedef int8_t ysw_mapper_item_t; // ysw_mapper interprets a target value of zero as a no-op

void ysw_mapper_create_task(ysw_bus_t *bus, const ysw_mapper_item_t *map);

