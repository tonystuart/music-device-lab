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

typedef struct {
    int16_t row;
    int16_t cs_index;
} selection_t;

typedef struct {
    ysw_music_t *music;
    ysw_frame_t *frame;
    lv_obj_t *table;
    selection_t selection;
} ysw_csl_t;

ysw_csl_t *ysw_csl_create(ysw_music_t *music);
void ysw_csl_set_cs(ysw_csl_t *csl, ysw_cs_t *cs);
