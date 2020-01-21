// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "esp_log.h"
#include "ysw_common.h"
#include "ysw_midi.h"

// https://en.wikipedia.org/wiki/Degree_(music)
// "In set theory, for instance, the 12 degrees of the chromatic scale usually are
// numbered starting from C=0, the twelve pitch classes being numbered from 0 to 11."

typedef struct PACKED song {
    char magic[4];
    uint8_t lowest_compatible_version;
    uint8_t current_version;
    int8_t easy_transposition;
    uint8_t tonic;
    uint8_t mode;
    uint16_t duration_in_seconds;
    uint16_t time_signature_count;
    uint16_t tempo_count;
    uint16_t part_count;
    uint32_t note_count;
    uint16_t lyric_count;
    uint16_t year;
    uint32_t genre_offset;
    uint32_t artist_offset;
    uint32_t title_offset;
    uint32_t time_signature_offsets;
    uint32_t tempo_offsets;
    uint32_t part_offsets;
    uint32_t note_offsets;
    uint32_t lyric_offsets;
} song_t;

typedef struct PACKED time_signature {
    uint32_t time;
    uint8_t beats_per_measure;
    uint8_t beat_unit;
} time_signature_t;

typedef struct PACKED tempo {
    uint32_t time;
    uint8_t quarter_notes_per_minute;
} tempo_t;

typedef struct PACKED part {
    uint8_t channel_index;
    uint8_t percent_measures;
    uint8_t percent_melody;
    uint8_t distinct_note_count;
    uint8_t occupancy;
    uint16_t concurrency;
} part_t;

typedef struct PACKED note {
    uint32_t time;
    uint16_t duration;
    uint8_t channel;
    uint8_t midi_note;
    uint8_t velocity;
    uint8_t instrument;
} note_t;

typedef struct PACKED lyric {
    uint32_t time;
    uint32_t lyric_offset;
} lyric_t;

char *ysw_song_get_tonic_name(uint8_t tonic);
char *ysw_song_get_mode_name(uint8_t mode);
char *ysw_song_get_note_name(uint8_t midi_note, char buf[4]);
void ysw_song_validate(song_t *song);
void ysw_song_dump(song_t *song);
uint8_t ysw_song_transpose(uint8_t midi_note);
void ysw_song_set_transposition(uint8_t new_transposition);
uint8_t ysw_song_get_transposition();

static inline char *ysw_song_get_title(song_t *song)
{
    return ((char*)song) + song->title_offset;
}

static inline time_signature_t *ysw_song_get_time_signatures(song_t *song)
{
    return (time_signature_t *)(((char*)song) + song->time_signature_offsets);
}

static inline tempo_t *ysw_song_get_tempos(song_t *song)
{
    return (tempo_t *)(((char*)song) + song->tempo_offsets);
}

static inline part_t *ysw_song_get_parts(song_t *song)
{
    return (part_t *)(((char*)song) + song->part_offsets);
}

static inline uint8_t ysw_song_get_channel(song_t *song, uint8_t part_index)
{
    return ysw_song_get_parts(song)[part_index].channel_index;
}

static inline note_t *ysw_song_get_notes(song_t *song)
{
    return (note_t *)(((char*)song) + song->note_offsets);
}

static inline note_t *ysw_song_get_note(song_t *song, uint32_t note_index)
{
    return &ysw_song_get_notes(song)[note_index];
}

static inline lyric_t *ysw_song_get_lyrics(song_t *song)
{
    return (lyric_t *)(((char*)song) + song->lyric_offsets);
}

static inline char *ysw_song_get_lyric_text(song_t *song, int16_t lyric_index)
{
    return ((char*)song) + ysw_song_get_lyrics(song)[lyric_index].lyric_offset;
}

static inline uint16_t ysw_song_to_angle(uint8_t midi_note)
{
    if (midi_note < LOWEST) {
        midi_note = LOWEST;
    } else if (midi_note > HIGHEST) {
        midi_note = HIGHEST;
    }
    uint8_t normalized = midi_note - LOWEST;
    uint8_t range = HIGHEST - LOWEST;
    uint16_t angle = (normalized * 360) / range;
    return angle;
}

