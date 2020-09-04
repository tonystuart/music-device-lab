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
#include "ysw_note.h"
#include "ysw_sn.h"
#include "ysw_cs.h"
#include "ysw_hp.h"

#define YSW_MUSIC_PARTITION "/spiffs"
#define YSW_MUSIC_CSV YSW_MUSIC_PARTITION "/music.csv"
#define YSW_MUSIC_TEMP YSW_MUSIC_PARTITION "/music.tmp"
#define YSW_MUSIC_SOUNDFONT YSW_MUSIC_PARTITION "/music.sf2"

typedef struct {
    ysw_array_t *cs_array;
    ysw_array_t *hp_array;
} ysw_music_t;

uint32_t ysw_music_get_cs_count(ysw_music_t *music);
ysw_cs_t *ysw_music_get_cs(ysw_music_t *music, uint32_t index);
int32_t ysw_music_get_cs_index(ysw_music_t *music, ysw_cs_t *cs);
int32_t ysw_music_get_hp_index(ysw_music_t *music, ysw_hp_t *hp);
void ysw_music_remove_cs(ysw_music_t *music, uint32_t index);
void ysw_music_remove_hp(ysw_music_t *music, uint32_t index);
uint32_t ysw_music_get_hp_count(ysw_music_t *music);
ysw_hp_t *ysw_music_get_hp(ysw_music_t *music, uint32_t index);
uint32_t ysw_music_insert_cs(ysw_music_t *music, ysw_cs_t *cs);
uint32_t ysw_music_insert_hp(ysw_music_t *music, ysw_hp_t *hp);
void ysw_music_dump(ysw_music_t *music, char *tag);
void ysw_music_sort_cs_by_name(ysw_music_t *music);
void ysw_music_sort_hp_by_name(ysw_music_t *music);
