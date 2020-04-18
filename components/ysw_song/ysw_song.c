// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_song.h"

#include "stdio.h"

#define TAG "SONG"
#define MAGIC "SONG"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)
#define VERSION 1

static int8_t current_transposition;

static char *tonics[] = {
    "C",
    "C#",
    "D",
    "D#",
    "E",
    "F",
    "F#",
    "G",
    "G#",
    "A",
    "A#",
    "B"
};

#define TONIC_COUNT (sizeof(tonics) / sizeof(char*))

static char *modes[] = {
    "Major", // aka Ionian
    "Dorian",
    "Phrygian",
    "Lydian",
    "Mixolydian",
    "minor", // aka Aeolian
    "Locrian",
};

#define MODE_COUNT (sizeof(modes) / sizeof(char*))

char *ysw_song_get_tonic_name(uint8_t tonic)
{
    return tonic < TONIC_COUNT ? tonics[tonic] : "n/a";
}

char *ysw_song_get_mode_name(uint8_t mode)
{
    return mode < MODE_COUNT ? modes[mode] : "n/a";
}

static char *notes[] = {
    "C",
    "C#",
    "D",
    "D#",
    "E",
    "F",
    "F#",
    "G",
    "G#",
    "A",
    "A#",
    "B",
};

char *ysw_song_get_note_name(uint8_t midi_note, char buf[4])
{
    uint8_t octave = midi_note / YSW_MIDI_USPO;
    uint8_t note = midi_note % YSW_MIDI_USPO;
    sprintf(buf, "%s%d", notes[note], octave);
    return (buf);
}

void ysw_song_validate(song_t *song)
{
    if (memcmp(song->magic, MAGIC, MAGIC_SIZE) != 0 || song->lowest_compatible_version > VERSION) {
        ESP_LOGE(TAG, "Unsupported song file format");
        abort();
    }
}

void ysw_song_dump(song_t *song)
{
    ESP_LOGD(TAG, "magic[0]=%c", song->magic[0]);
    ESP_LOGD(TAG, "magic[1]=%c", song->magic[1]);
    ESP_LOGD(TAG, "magic[2]=%c", song->magic[2]);
    ESP_LOGD(TAG, "magic[3]=%c", song->magic[3]);
    ESP_LOGD(TAG, "lowest_compatible_version=%d", song->lowest_compatible_version);
    ESP_LOGD(TAG, "current_version=%d", song->current_version);
    ESP_LOGD(TAG, "easy_transposition=%d", song->easy_transposition);
    ESP_LOGD(TAG, "tonic=%d", song->tonic);
    ESP_LOGD(TAG, "mode=%d", song->mode);
    ESP_LOGD(TAG, "duration_in_seconds=%d", song->duration_in_seconds);
    ESP_LOGD(TAG, "time_signature_count=%d", song->time_signature_count);
    ESP_LOGD(TAG, "tempo_count=%d", song->tempo_count);
    ESP_LOGD(TAG, "part_count=%d", song->part_count);
    ESP_LOGD(TAG, "note_count=%d", song->note_count);
    ESP_LOGD(TAG, "lyric_count=%d", song->lyric_count);
    ESP_LOGD(TAG, "year=%d", song->year);
    ESP_LOGD(TAG, "genre_offset=%d", song->genre_offset);
    ESP_LOGD(TAG, "artist_offset=%d", song->artist_offset);
    ESP_LOGD(TAG, "title_offset=%d", song->title_offset);
    ESP_LOGD(TAG, "part_offsets=%d", song->part_offsets);
    ESP_LOGD(TAG, "note_offsets=%d", song->note_offsets);
    ESP_LOGD(TAG, "lyric_offsets=%d", song->lyric_offsets);

    time_signature_t *time_signatures = ysw_song_get_time_signatures(song);
    for (int i = 0; i < song->time_signature_count; i++) {
        time_signature_t *time_signature = &time_signatures[i];
        ESP_LOGD(TAG, "time_signature=%d", i);
        ESP_LOGD(TAG, "time=%d", time_signature->time);
        ESP_LOGD(TAG, "beats_per_measure=%d", time_signature->beats_per_measure);
        ESP_LOGD(TAG, "beat_unit=%d", time_signature->beat_unit);
    }

    tempo_t *tempos = ysw_song_get_tempos(song);
    for (int i = 0; i < song->tempo_count; i++) {
        tempo_t *tempo = &tempos[i];
        ESP_LOGD(TAG, "tempo=%d", i);
        ESP_LOGD(TAG, "time=%d", tempo->time);
        ESP_LOGD(TAG, "quarter_notes_per_minute=%d", tempo->quarter_notes_per_minute);
    }

    part_t *parts = ysw_song_get_parts(song);
    for (int i = 0; i < song->part_count; i++) {
        part_t *part = &parts[i];
        ESP_LOGD(TAG, "part=%d", i);
        ESP_LOGD(TAG, "channel=%d", part->channel_index);
        ESP_LOGD(TAG, "percent_measures=%d", part->percent_measures);
        ESP_LOGD(TAG, "percent_melody=%d", part->percent_melody);
        ESP_LOGD(TAG, "distinct_note_count=%d", part->distinct_note_count);
        ESP_LOGD(TAG, "occupancy=%d", part->occupancy);
        ESP_LOGD(TAG, "concurrency=%d", part->concurrency);
    }
}

uint8_t ysw_song_transpose(uint8_t midi_note)
{
    return midi_note + current_transposition;
}

void ysw_song_set_transposition(uint8_t new_transposition)
{
    current_transposition = new_transposition;
}

uint8_t ysw_song_get_transposition()
{
    return current_transposition;
}

