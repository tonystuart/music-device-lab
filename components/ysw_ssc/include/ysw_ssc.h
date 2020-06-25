// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_hp.h"
#include "ysw_music.h"
#include "ysw_sdb.h"
#include "lvgl.h"
#include "stdint.h"

typedef struct {
    ysw_music_t *music;
    ysw_hp_t *hp;
    ysw_ps_t *ps;
} ysw_ssc_model_t;

typedef struct {
    ysw_sdb_t *sdb;
    lv_obj_t *som;
    lv_obj_t *degree;
    lv_obj_t *styles;
} ysw_ssc_view_t;

typedef struct {
    ysw_ssc_model_t model;
    ysw_ssc_view_t view;
} ysw_ssc_t;

void ysw_ssc_create(ysw_music_t *music, ysw_hp_t *hp, uint32_t ps_index);
void ysw_ssc_close(ysw_ssc_t *ssc);
