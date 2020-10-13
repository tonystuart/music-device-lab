// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_event.h"
#include "ysw_bus.h"
#include "esp_log.h"

#define TAG "EVENT"

ysw_bus_h ysw_event_create_bus()
{
    return ysw_bus_create(YSW_ORIGIN_LAST, 4, 16, sizeof(ysw_event_t));
}

void ysw_event_publish(ysw_bus_h bus, ysw_event_t *event)
{
    ysw_bus_publish(bus, event->header.origin, event, sizeof(ysw_event_t));
}
