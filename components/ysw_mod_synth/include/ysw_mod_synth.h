// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "stdint.h"
#include "ysw_bus.h"

typedef enum {
    YSW_MOD_PAN_LEFT,
    YSW_MOD_PAN_CENTER,
    YSW_MOD_PAN_RIGHT,
} ysw_mod_pan_t;

typedef struct {
    int8_t  *data;
    uint16_t   length;
    uint16_t   reppnt;
    uint16_t   replen;
    uint8_t  volume;
    ysw_mod_pan_t pan;
} ysw_mod_sample_t;

typedef ysw_mod_sample_t *(*ysw_mod_sample_provider_t)(void *host_context, uint8_t program, uint8_t note);

typedef struct {
    void* host_context;
    ysw_mod_sample_provider_t provide_sample;
} ysw_mod_host_t;

void ysw_mod_synth_create_task(ysw_bus_h bus, ysw_mod_host_t *mod_host);
