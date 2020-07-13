// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_cs.h"
#include "ysw_music.h"
#include "ysw_sdb.h"
#include "lvgl.h"
#include "stdint.h"

typedef struct {
    ysw_music_t *music;
    ysw_cs_t *cs;
    ysw_sn_t *sn;
    bool apply_all;
} ysw_nsc_model_t;

typedef struct {
    ysw_sdb_t *sdb;
    lv_obj_t *start;
    lv_obj_t *duration;
    lv_obj_t *velocity;
    lv_obj_t *quatone;
} ysw_nsc_view_t;

typedef struct {
    ysw_nsc_model_t model;
    ysw_nsc_view_t view;
} ysw_nsc_t;

void ysw_nsc_create(ysw_music_t *music, ysw_cs_t *cs, uint32_t sn_index, bool apply_all);
void ysw_nsc_close(ysw_nsc_t *nsc);
