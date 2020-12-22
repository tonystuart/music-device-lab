// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_bus.h"
#include "zm_music.h"
#include "lvgl.h"

typedef struct {
    lv_obj_t *container;
    lv_obj_t *label;
    lv_obj_t *image;
} ysw_splash_t;

void ysw_shell_create(ysw_bus_t *bus, zm_music_t *music);
