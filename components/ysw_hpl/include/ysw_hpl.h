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

typedef struct ysw_hpl_s ysw_hpl_t;

typedef void (*hpl_close_cb_t)(void *context, ysw_hpl_t *hpl);

typedef struct {
    ysw_music_t *music;
    uint32_t hp_index;
} ysw_hpl_model_t;

typedef struct {
    ysw_hp_t *clipboard_hp;
} ysw_hpl_controller_t;

typedef struct ysw_hpl_s {
    ysw_hpl_model_t model;
    ysw_ui_frame_t frame;
    ysw_hpl_controller_t controller;
} ysw_hpl_t;

ysw_hpl_t* ysw_hpl_create(ysw_music_t *music, uint32_t hp_index);
void ysw_hpl_close(ysw_hpl_t *hpl);
