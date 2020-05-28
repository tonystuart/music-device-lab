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

typedef struct cpc_s cpc_t;

typedef void (*cpc_close_cb_t)(void *context, cpc_t *cpc);

typedef struct cpc_s {
    ysw_frame_t *frame;
    lv_obj_t *cpe;
    ysw_music_t *music;
    ysw_array_t *clipboard;
    uint32_t cp_index;
    int32_t step_index;
    cpc_close_cb_t close_cb;
    void *close_cb_context;
} cpc_t;

cpc_t *cpc_create(ysw_music_t *new_music, uint32_t new_cs_index);
void cpc_set_close_cb(cpc_t *cpc, cpc_close_cb_t cb, void *context);
void cpc_on_metro(cpc_t *cpc, note_t *metro_note);

