// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_array.h"
#include "stdint.h"

typedef struct {
    ysw_array_t *listeners;
} ysw_bus_t;

typedef void (*ysw_bus_cb_t)(void *context, uint32_t evt, void *details, char *sender);

ysw_bus_t* ysw_bus_create(uint32_t initial_size);
uint32_t ysw_bus_subscribe(ysw_bus_t *bus, void *cb, void *context);
uint32_t ysw_bus_publish(ysw_bus_t *bus, uint32_t evt, void *details);
void ysw_bus_unsubscribe(ysw_bus_t *bus, uint32_t handle);
void ysw_bus_free(ysw_bus_t *bus);
