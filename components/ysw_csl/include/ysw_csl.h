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
#include "lvgl.h"

void *ysw_csl_create(lv_obj_t *parent, ysw_music_t *music, uint32_t cs_index);
