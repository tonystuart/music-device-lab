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

typedef struct csc_s csc_t;

typedef void (*csc_close_cb_t)(void *context, csc_t *csc);

typedef struct csc_s {
    ysw_frame_t *frame;
    lv_obj_t *cse;
    ysw_music_t *music;
    uint32_t cs_index;
    ysw_array_t *clipboard;
    csc_close_cb_t close_cb;
    void *close_cb_context;
} csc_t;

csc_t *csc_create(ysw_music_t *music, uint32_t cs_index);
csc_t *csc_create_new(ysw_music_t *music, uint32_t old_cs_index);
void csc_set_close_cb(csc_t *csc, void *cb, void *context);
void csc_on_metro(csc_t *csc, note_t *metro_note);

