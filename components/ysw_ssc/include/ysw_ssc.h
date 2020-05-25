// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_cp.h"
#include "ysw_music.h"
#include "stdint.h"

#if 0
typedef struct {
    ysw_sdb_choice_cb_t *chord_style_cb;
    ysw_sdb_checkbox_cb_t *new_measure_cb;
} ysw_ssc_config_t;
#endif

void ysw_ssc_create(ysw_music_t *music, ysw_cp_t *cp, uint32_t step_index);

