// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_array.h"
#include "ysw_song.h"
#include "ysw_cn.h"
#include "ysw_cs.h"
#include "ysw_cp.h"

typedef struct {
    ysw_array_t *cs_array;
    ysw_array_t *cp_array;
} ysw_music_t;

uint32_t ysw_music_get_cs_count(ysw_music_t *music);
ysw_cs_t *ysw_music_get_cs(ysw_music_t *music, uint32_t index);
int32_t ysw_music_get_cs_index(ysw_music_t *music, ysw_cs_t *cs);
uint32_t ysw_music_get_cp_count(ysw_music_t *music);
ysw_cp_t *ysw_music_get_cp(ysw_music_t *music, uint32_t index);
void ysw_music_insert_cs(ysw_music_t *music, uint32_t index, ysw_cs_t *cs);
void ysw_music_insert_cp(ysw_music_t *music, uint32_t index, ysw_cp_t *cp);
void ysw_music_dump(ysw_music_t *music, char *tag);
