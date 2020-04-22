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

uint32_t ysw_music_get_chord_count(ysw_music_t *music)
{
    return ysw_array_get_count(music->chords);
}

ysw_chord_t *ysw_music_get_chord(ysw_music_t *music, uint32_t index)
{
    return ysw_array_get(music->chords, index);
}

uint32_t ysw_music_get_progression_count(ysw_music_t *music)
{
    return ysw_array_get_count(music->progressions);
}

ysw_progression_t *ysw_music_get_progression(ysw_music_t *music, uint32_t index)
{
    return ysw_array_get(music->progressions, index);
}

void ysw_music_dump(ysw_music_t *music, char *tag)
{
    uint32_t chord_count = ysw_music_get_chord_count(music);
    for (uint32_t i = 0; i < chord_count; i++) {
        ysw_chord_t *chord = ysw_music_get_chord(music, i);
        ESP_LOGI(tag, "chord[%d]=%s", i, chord->name);
        ysw_chord_dump(chord, tag);
    }

    uint32_t progression_count = ysw_music_get_progression_count(music);
    for (uint32_t i = 0; i < progression_count; i++) {
        ysw_progression_t *progression = ysw_music_get_progression(music, i);
        ESP_LOGI(tag, "progression[%d]=%s", i, progression->name);
        ysw_progression_dump(progression, tag);
    }
}