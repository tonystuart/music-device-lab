// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_music.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_MUSIC"

uint32_t ysw_music_get_cs_count(ysw_music_t *music)
{
    return ysw_array_get_count(music->cs_array);
}

ysw_cs_t *ysw_music_get_cs(ysw_music_t *music, uint32_t index)
{
    return ysw_array_get(music->cs_array, index);
}

int32_t ysw_music_get_cs_index(ysw_music_t *music, ysw_cs_t *cs)
{
    return ysw_array_find(music->cs_array, cs);
}

void ysw_music_remove_cs(ysw_music_t *music, uint32_t index)
{
    ysw_array_remove(music->cs_array, index);
}

uint32_t ysw_music_get_hp_count(ysw_music_t *music)
{
    return ysw_array_get_count(music->hp_array);
}

ysw_hp_t *ysw_music_get_hp(ysw_music_t *music, uint32_t index)
{
    return ysw_array_get(music->hp_array, index);
}

uint32_t ysw_music_insert_cs(ysw_music_t *music, ysw_cs_t *cs)
{
    ysw_array_push(music->cs_array, cs);
    ysw_music_sort_cs_by_name(music);
    return ysw_music_get_cs_index(music, cs);
}

void ysw_music_insert_hp(ysw_music_t *music, uint32_t index, ysw_hp_t *hp)
{
    ysw_array_insert(music->hp_array, index, hp);
}

static int compare_cs_name(const void *left, const void *right)
{
    const ysw_cs_t *left_cs = *(ysw_cs_t * const *)left;
    const ysw_cs_t *right_cs = *(ysw_cs_t * const *)right;
    int delta = strcmp(left_cs->name, right_cs->name);
    return delta;
}

void ysw_music_sort_cs_by_name(ysw_music_t *music)
{
    ysw_array_sort(music->cs_array, compare_cs_name);
}
