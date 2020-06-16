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
#include "ysw_frame.h"
#include "ysw_music.h"
#include "stdint.h"

typedef struct hpc_s ysw_hpc_t;

typedef void (*hpc_close_cb_t)(void *context, ysw_hpc_t *hpc);

typedef struct hpc_s {
    ysw_frame_t *frame;
    lv_obj_t *hpe;
    ysw_music_t *music;
    ysw_array_t *clipboard;
    uint32_t hp_index;
    uint32_t ps_index;
    hpc_close_cb_t close_cb;
    void *close_cb_context;
} ysw_hpc_t;

ysw_hpc_t *ysw_hpc_create(ysw_music_t *new_music, uint32_t new_cs_index);
void ysw_hpc_set_close_cb(ysw_hpc_t *hpc, hpc_close_cb_t cb, void *context);

