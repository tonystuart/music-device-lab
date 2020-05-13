// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_music.h"

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

uint32_t ysw_music_get_cp_count(ysw_music_t *music)
{
    return ysw_array_get_count(music->cp_array);
}

ysw_cp_t *ysw_music_get_cp(ysw_music_t *music, uint32_t index)
{
    return ysw_array_get(music->cp_array, index);
}

void ysw_music_insert_cs(ysw_music_t *music, uint32_t index, ysw_cs_t *cs)
{
    ysw_array_insert(music->cs_array, index, cs);
}

void ysw_music_insert_cp(ysw_music_t *music, uint32_t index, ysw_cp_t *cp)
{
    ysw_array_insert(music->cp_array, index, cp);
}

void ysw_music_dump(ysw_music_t *music, char *tag)
{
    uint32_t cs_count = ysw_music_get_cs_count(music);
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_cs_t *cs = ysw_music_get_cs(music, i);
        ESP_LOGI(tag, "cs[%d]=%s", i, cs->name);
        ysw_cs_dump(cs, tag);
    }

    uint32_t cp_count = ysw_music_get_cp_count(music);
    for (uint32_t i = 0; i < cp_count; i++) {
        ysw_cp_t *cp = ysw_music_get_cp(music, i);
        ESP_LOGI(tag, "cp[%d]=%s", i, cp->name);
        ysw_cp_dump(cp, tag);
    }
}
