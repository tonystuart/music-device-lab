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
#include "ysw_heap.h"

#define TAG "YSW_MUSIC"

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

ysw_chord_t *ysw_chord_create(char *name, uint32_t duration)
{
    ysw_chord_t *chord = ysw_heap_allocate(sizeof(ysw_chord_t));
    chord->name = ysw_heap_strdup(name);
    chord->chord_notes = ysw_array_create(8);
    chord->duration = duration;
    return chord;
}

void ysw_chord_free(ysw_chord_t *chord)
{
    assert(chord);
    ysw_array_free(chord->chord_notes);
    ysw_heap_free(chord);
}

int ysw_chord_add_note(ysw_chord_t *chord, ysw_chord_note_t *chord_note)
{
    assert(chord);
    assert(chord_note);
    chord->duration = max(chord->duration, chord_note->time + chord_note->duration);
    int index = ysw_array_push(chord->chord_notes, chord_note);
    return index;
}

void ysw_chord_set_duration(ysw_chord_t *chord, uint32_t duration)
{
    assert(chord);
    chord->duration = duration;
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

ysw_progression_t *ysw_progression_create()
{
    ysw_progression_t *progression = ysw_heap_allocate(sizeof(ysw_progression_t));
    progression->chords = ysw_array_create(8);
    progression->instrument = 0;
    progression->percent_tempo = 100;
    progression->tonic = 60;
    ESP_LOGD(TAG, "create progression=%p", progression);
    return progression;
}

void ysw_progression_free(ysw_progression_t *progression)
{
    assert(progression);
    ESP_LOGD(TAG, "free progression=%p, chords=%d", progression, ysw_array_get_count(progression->chords));
    int chord_count = ysw_array_get_count(progression->chords);
    for (int i = 0; i < chord_count; i++) {
        ysw_progression_chord_t *progression_chord = ysw_array_get(progression->chords, i);
        ysw_heap_free(progression_chord);
    }
    ysw_array_free(progression->chords);
    ysw_heap_free(progression);
}

int ysw_progression_add_chord(ysw_progression_t *progression, uint8_t degree, ysw_chord_t *chord)
{
    assert(progression);
    assert(degree < YSW_MIDI_MAX);
    assert(chord);
    ysw_progression_chord_t *progression_chord = ysw_heap_allocate(sizeof(ysw_progression_chord_t));
    progression_chord->degree = degree;
    int index = ysw_array_push(progression->chords, chord);
    return index;
}

void ysw_progression_set_tonic(ysw_progression_t *progression, uint8_t tonic)
{
    assert(progression);
    assert(tonic < YSW_MIDI_MAX);
    progression->tonic = tonic;
}

void ysw_progression_set_instrument(ysw_progression_t *progression, uint8_t instrument)
{
    assert(progression);
    assert(instrument < YSW_MIDI_MAX);
    progression->instrument = instrument;
}

void ysw_progression_set_percent_tempo(ysw_progression_t *progression, uint8_t percent_tempo)
{
    assert(progression);
    progression->percent_tempo = percent_tempo;
}

uint32_t ysw_get_note_count(ysw_progression_t *progression)
{
    assert(progression);
    uint32_t note_count = 0;
    int chord_count = ysw_array_get_count(progression->chords);
    for (int i = 0; i < chord_count; i++) {
        ysw_progression_chord_t *progression_chord = ysw_array_get(progression->chords, i);
        int chord_note_count = ysw_array_get_count(progression_chord->chord->chord_notes);
        note_count += chord_note_count;
    }
    return note_count;
}

note_t *ysw_progression_get_notes(ysw_progression_t *progression)
{
    assert(progression);
    int chord_time = 0;
    int chord_count = ysw_array_get_count(progression->chords);
    int note_count = ysw_get_note_count(progression);
    note_t *notes = ysw_heap_allocate(sizeof(note_t) * note_count);
    note_t *note_p = notes;
    for (int i = 0; i < chord_count; i++) {
        ysw_progression_chord_t *progression_chord = ysw_array_get(progression->chords, i);
        uint8_t chord_root = progression_chord->degree;
        int chord_note_count = ysw_array_get_count(progression_chord->chord->chord_notes);
        for (int j = 0; j < chord_note_count; j++) {
            ysw_chord_note_t *chord_note = ysw_array_get(progression_chord->chord->chord_notes, j);
            note_p->time = chord_time + chord_note->time;
            note_p->duration = chord_note->duration;
            note_p->channel = 0;
            note_p->midi_note = to_note(progression->tonic, chord_root, chord_note->degree);
            note_p->velocity = chord_note->velocity;
            note_p->instrument = progression->instrument;
            ESP_LOGD(TAG, "time=%u, duration=%d, midi_note=%d, velocity=%d, instrument=%d", note_p->time, note_p->duration, note_p->midi_note, note_p->velocity, note_p->instrument);
            note_p++;
        }
        chord_time += progression_chord->chord->duration;
    }
    return notes;
}

