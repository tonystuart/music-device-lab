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
#include "ysw_music.h"
#include "ysw_ui.h"
#include "stdint.h"

typedef struct hpc_s ysw_hpc_t;

typedef void (*hpc_close_cb_t)(void *context, uint32_t hp_index);

typedef struct {
    ysw_music_t *music;
    uint32_t hp_index;
    uint32_t ps_index;
    ysw_array_t *clipboard;
    hpc_close_cb_t close_cb;
    void *close_cb_context;
    lv_obj_t *hpe;
} ysw_hpc_controller_t;

typedef struct hpc_s {
    ysw_ui_frame_t frame;
    ysw_hpc_controller_t controller;
} ysw_hpc_t;

typedef enum {
    YSW_HPC_CREATE_HP,
    YSW_HPC_EDIT_HP,
} ysw_hpc_type_t;

ysw_hpc_t* ysw_hpc_create(ysw_music_t *new_music, uint32_t new_cs_index, ysw_hpc_type_t type);
void ysw_hpc_set_close_cb(ysw_hpc_t *hpc, hpc_close_cb_t cb, void *context);

