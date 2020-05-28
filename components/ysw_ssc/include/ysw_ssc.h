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
#include "stdint.h"

typedef struct {
    ysw_music_t *music;
    ysw_hp_t *hp;
    ysw_step_t *step;
} ssc_t;

void ysw_ssc_create(ysw_music_t *music, ysw_hp_t *hp, uint32_t step_index);

