// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_main_bus.h"

static ysw_bus_t *ysw_main_bus;

void ysw_main_bus_create()
{
    ysw_main_bus = ysw_bus_create(8);
}

uint32_t ysw_main_bus_subscribe(void *cb, void *context)
{
    return ysw_bus_subscribe(ysw_main_bus, cb, context);
}

uint32_t ysw_main_bus_publish(ysw_bus_evt_t msg, void *details)
{
    return ysw_bus_publish(ysw_main_bus, msg, details);
}

void ysw_main_bus_unsubscribe(uint32_t handle)
{
    ysw_bus_unsubscribe(ysw_main_bus, handle);
}
