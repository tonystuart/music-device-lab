// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_clip.h"

#define TAG "YSW_CLIP"

// TODO: Change type of MIDI 7 bit quantities to int8_t

static inline int8_t to_note(int8_t tonic_index, int8_t root_number, int8_t degree_number)
{
    static uint8_t intervals[7][7] = {
        /* C */ { 0, 2, 4, 5, 7, 9, 11 },
        /* D */ { 0, 2, 3, 5, 7, 9, 10 },
        /* E */ { 0, 1, 3, 5, 7, 8, 10 },
        /* F */ { 0, 2, 4, 6, 7, 9, 11 },
        /* G */ { 0, 2, 4, 5, 7, 9, 10 },
        /* A */ { 0, 2, 3, 5, 7, 8, 10 },
        /* B */ { 0, 1, 3, 5, 6, 8, 10 },
    };

    int8_t root_semitones;
    if (root_number < 0) {
        root_semitones = -1;
        root_number = -root_number;
    } else {
        root_semitones = 0;
    }

    int8_t degree_semitones;
    if (degree_number < 0) {
        degree_semitones = -1;
        degree_number = -degree_number;
    } else {
        degree_semitones = 0;
    }

    int8_t root_offset = root_number - 1;
    int8_t root_octave = root_offset / 7;
    int8_t root_index = root_offset % 7;
    int8_t root_interval = intervals[0][root_index];

    int8_t degree_offset = degree_number - 1;
    int8_t degree_octave = degree_offset / 7;
    int8_t degree_index = degree_offset % 7;
    int8_t degree_interval = intervals[root_index][degree_index];

    int8_t note = tonic_index +
        ((12 * root_octave) + root_interval + root_semitones) +
        ((12 * degree_octave) + degree_interval + degree_semitones);

    ESP_LOGD(TAG, "tonic_index=%d, root_number=%d, degree_number=%d, root_interval=%d, degree_interval=%d, note=%d", tonic_index, root_number, degree_number, root_interval, degree_interval, note);

    return note;
}

ysw_chord_style_t *ysw_chord_style_create(char *name, uint32_t duration)
{
    ysw_chord_style_t *chord_style = ysw_heap_allocate(sizeof(ysw_chord_style_t));
    chord_style->name = ysw_heap_strdup(name);
    chord_style->chord_notes = ysw_array_create(8);
    chord_style->duration = duration;
    return chord_style;
}

void ysw_chord_style_free(ysw_chord_style_t *chord_style)
{
    assert(chord_style);
    ysw_array_free(chord_style->chord_notes);
    ysw_heap_free(chord_style);
}

int ysw_chord_style_add_note(ysw_chord_style_t *chord_style, ysw_chord_note_t *chord_note)
{
    assert(chord_style);
    assert(chord_note);
    chord_style->duration = max(chord_style->duration, chord_note->time + chord_note->duration);
    int index = ysw_array_push(chord_style->chord_notes, chord_note);
    return index;
}

void ysw_chord_style_set_duration(ysw_chord_style_t *chord_style, uint32_t duration)
{
    assert(chord_style);
    chord_style->duration = duration;
}

ysw_chord_note_t *ysw_chord_note_create(uint8_t degree, uint8_t velocity, uint32_t time, uint32_t duration)
{
    ESP_LOGD(TAG, "chord_note_create degree=%u, velocity=%u, time=%u, duration=%u", degree, velocity, time, duration);
    ysw_chord_note_t *ysw_chord_note = ysw_heap_allocate(sizeof(ysw_chord_note_t));
    ysw_chord_note->time = time;
    ysw_chord_note->duration = duration;
    ysw_chord_note->degree = degree;
    ysw_chord_note->velocity = velocity;
    return ysw_chord_note;
}

void ysw_chord_note_free(ysw_chord_note_t *ysw_chord_note)
{
    assert(ysw_chord_note);
    ESP_LOGD(TAG, "free chord_note=%p, time=%u, degree=%u, chord_note=%u, velocity=%u", ysw_chord_note, ysw_chord_note->time, ysw_chord_note->degree, ysw_chord_note->degree, ysw_chord_note->velocity);
    ysw_heap_free(ysw_chord_note);
}

ysw_clip_t *ysw_clip_create()
{
    ysw_clip_t *clip = ysw_heap_allocate(sizeof(ysw_clip_t));
    clip->chords = ysw_array_create(8);
    clip->chord_style = NULL;
    clip->instrument = 0;
    clip->percent_tempo = 100;
    clip->tonic = 60;
    ESP_LOGD(TAG, "create clip=%p", clip);
    return clip;
}

void ysw_clip_free(ysw_clip_t *clip)
{
    assert(clip);
    ESP_LOGD(TAG, "free clip=%p, chords=%d", clip, ysw_array_get_count(clip->chords));
    int chord_count = ysw_array_get_count(clip->chords);
    for (int i = 0; i < chord_count; i++) {
        ysw_chord_t *chord = ysw_array_get(clip->chords, i);
        ysw_heap_free(chord);
    }
    ysw_array_free(clip->chords);
    ysw_heap_free(clip);
}

int ysw_clip_add_chord_with_style(ysw_clip_t *clip, uint8_t chord_number, ysw_chord_style_t *chord_style)
{
    assert(clip);
    assert(chord_number < YSW_MIDI_MAX);
    assert(chord_style);
    ysw_chord_t *chord = ysw_heap_allocate(sizeof(ysw_chord_t));
    chord->chord_number = chord_number;
    chord->chord_style = chord_style;
    int index = ysw_array_push(clip->chords, chord);
    return index;
}

int ysw_clip_add_chord(ysw_clip_t *clip, uint8_t chord_number)
{
    assert(clip);
    assert(chord_number < YSW_MIDI_MAX);
    return ysw_clip_add_chord_with_style(clip, chord_number, clip->chord_style);
}

void ysw_clip_set_chord_style(ysw_clip_t *clip, ysw_chord_style_t *chord_style)
{
    assert(clip);
    assert(chord_style);
    clip->chord_style = chord_style;
}

void ysw_clip_set_tonic(ysw_clip_t *clip, uint8_t tonic)
{
    assert(clip);
    assert(tonic < YSW_MIDI_MAX);
    clip->tonic = tonic;
}

void ysw_clip_set_instrument(ysw_clip_t *clip, uint8_t instrument)
{
    assert(clip);
    assert(instrument < YSW_MIDI_MAX);
    clip->instrument = instrument;
}

void ysw_clip_set_percent_tempo(ysw_clip_t *clip, uint8_t percent_tempo)
{
    assert(clip);
    clip->percent_tempo = percent_tempo;
}

uint32_t ysw_get_note_count(ysw_clip_t *clip)
{
    assert(clip);
    uint32_t note_count = 0;
    int chord_count = ysw_array_get_count(clip->chords);
    for (int i = 0; i < chord_count; i++) {
        ysw_chord_t *chord = ysw_array_get(clip->chords, i);
        int chord_note_count = ysw_array_get_count(chord->chord_style->chord_notes);
        note_count += chord_note_count;
    }
    return note_count;
}

note_t *ysw_clip_get_notes(ysw_clip_t *clip)
{
    assert(clip);
    int chord_time = 0;
    int chord_count = ysw_array_get_count(clip->chords);
    int note_count = ysw_get_note_count(clip);
    note_t *notes = ysw_heap_allocate(sizeof(note_t) * note_count);
    note_t *note_p = notes;
    for (int i = 0; i < chord_count; i++) {
        ysw_chord_t *chord = ysw_array_get(clip->chords, i);
        uint8_t chord_root = chord->chord_number;
        int chord_note_count = ysw_array_get_count(chord->chord_style->chord_notes);
        for (int j = 0; j < chord_note_count; j++) {
            ysw_chord_note_t *chord_note = ysw_array_get(chord->chord_style->chord_notes, j);
            note_p->time = chord_time + chord_note->time;
            note_p->duration = chord_note->duration;
            note_p->channel = 0;
            note_p->midi_note = to_note(clip->tonic, chord_root, chord_note->degree);
            note_p->velocity = chord_note->velocity;
            note_p->instrument = clip->instrument;
            ESP_LOGD(TAG, "time=%u, duration=%d, midi_note=%d, velocity=%d, instrument=%d", note_p->time, note_p->duration, note_p->midi_note, note_p->velocity, note_p->instrument);
            note_p++;
        }
        chord_time += chord->chord_style->duration;
    }
    return notes;
}

