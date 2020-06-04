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

void ysw_music_insert_cs(ysw_music_t *music, uint32_t index, ysw_cs_t *cs)
{
    ysw_array_insert(music->cs_array, index, cs);
}

void ysw_music_insert_hp(ysw_music_t *music, uint32_t index, ysw_hp_t *hp)
{
    ysw_array_insert(music->hp_array, index, hp);
}

void ysw_music_dump(ysw_music_t *music, char *tag)
{
    uint32_t cs_count = ysw_music_get_cs_count(music);
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_cs_t *cs = ysw_music_get_cs(music, i);
        ESP_LOGI(tag, "cs[%d]=%s", i, cs->name);
        ysw_cs_dump(cs, tag);
    }

    uint32_t hp_count = ysw_music_get_hp_count(music);
    for (uint32_t i = 0; i < hp_count; i++) {
        ysw_hp_t *hp = ysw_music_get_hp(music, i);
        ESP_LOGI(tag, "hp[%d]=%s", i, hp->name);
        ysw_hp_dump(hp, tag);
    }
}
