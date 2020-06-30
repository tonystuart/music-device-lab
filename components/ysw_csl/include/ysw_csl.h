// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_music.h"
#include "ysw_ui.h"
#include "lvgl.h"

typedef struct ysw_csl_s ysw_csl_t;

typedef void (*csl_close_cb_t)(void *context, ysw_csl_t *csl);

typedef struct {
    ysw_music_t *music;
    uint32_t cs_index;
} ysw_csl_model_t;

typedef struct {
    ysw_cs_t *clipboard_cs;
} ysw_csl_controller_t;

typedef struct ysw_csl_s {
    ysw_csl_model_t model;
    ysw_ui_frame_t frame;
    ysw_csl_controller_t controller;
} ysw_csl_t;

ysw_csl_t* ysw_csl_create(ysw_music_t *music, uint32_t cs_index);
void ysw_csl_close(ysw_csl_t *csl);
