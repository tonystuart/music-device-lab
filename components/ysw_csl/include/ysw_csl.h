// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "lvgl.h"
#include "ysw_array.h"
#include "ysw_bounds.h"
#include "ysw_cs.h"
#include "ysw_frame.h"
#include "ysw_music.h"

typedef struct ysw_csl_s ysw_csl_t;

typedef void (*csl_close_cb_t)(void *context, ysw_csl_t *csl);

typedef struct ysw_csl_s {
    ysw_music_t *music;
    uint32_t cs_index;
    ysw_frame_t *frame;
    lv_obj_t *table;
    csl_close_cb_t close_cb;
    void *close_cb_context;
    lv_point_t click_point;
    bool dragged;
} ysw_csl_t;

ysw_csl_t *ysw_csl_create(ysw_music_t *music);
void ysw_csl_set_cs(ysw_csl_t *csl, ysw_cs_t *cs);
