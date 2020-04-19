// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// TODO: Change type of MIDI 7 bit quantities to int8_t

#include "ysw_progression.h"

#include "assert.h"
#include "ysw_heap.h"

#define TAG "YSW_PROGRESSION"

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

ysw_progression_t *ysw_progression_create(char *name, uint8_t tonic, uint8_t instrument, uint8_t bpm)
{
    ESP_LOGD(TAG, "ysw_progression_create name=%s", name);
    ysw_progression_t *progression = ysw_heap_allocate(sizeof(ysw_progression_t));
    progression->name = ysw_heap_strdup(name);
    progression->slots = ysw_array_create(8);
    progression->instrument = instrument;
    progression->percent_tempo = 100;
    progression->tonic = tonic;
    return progression;
}

void ysw_progression_free(ysw_progression_t *progression)
{
    assert(progression);
    ESP_LOGD(TAG, "ysw_progression_free name=%s", progression->name);
    int chord_count = ysw_array_get_count(progression->slots);
    for (int i = 0; i < chord_count; i++) {
        ysw_slot_t *slot = ysw_array_get(progression->slots, i);
        ysw_heap_free(slot);
    }
    ysw_array_free(progression->slots);
    ysw_heap_free(progression->name);
    ysw_heap_free(progression);
}

int ysw_progression_add_chord(ysw_progression_t *progression, uint8_t degree, ysw_chord_t *chord)
{
    assert(progression);
    assert(degree < YSW_MIDI_MAX);
    assert(chord);
    ysw_slot_t *slot = ysw_heap_allocate(sizeof(ysw_slot_t));
    slot->degree = degree;
    slot->chord = chord;
    int index = ysw_array_push(progression->slots, slot);
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

void ysw_progression_dump(ysw_progression_t *progression, char *tag)
{
    ESP_LOGD(tag, "ysw_progression_dump progression=%p", progression);
    ESP_LOGD(tag, "name=%s", progression->name);
    ESP_LOGD(tag, "tonic=%d", progression->tonic);
    ESP_LOGD(tag, "instrument=%d", progression->instrument);
    ESP_LOGD(tag, "percent_tempo=%d", progression->percent_tempo);
    uint32_t chord_count = ysw_array_get_count(progression->slots);
    ESP_LOGD(tag, "chord_count=%d", chord_count);
    for (uint32_t i = 0; i < chord_count; i++) {
        ysw_slot_t *slot = ysw_array_get(progression->slots, i);
        ESP_LOGD(tag, "chord index=%d, degree=%d", i, slot->degree);
        ysw_chord_dump(slot->chord, tag);
    }
}

uint32_t ysw_progression_get_note_count(ysw_progression_t *progression)
{
    assert(progression);
    uint32_t note_count = 0;
    int chord_count = ysw_progression_get_chord_count(progression);
    for (int i = 0; i < chord_count; i++) {
        ESP_LOGD(TAG, "i=%d, chord_count=%d", i, chord_count);
        ysw_chord_t *chord = ysw_progression_get_chord(progression, i);
        int chord_note_count = ysw_array_get_count(chord->chord_notes);
        note_count += chord_note_count;
    }
    return note_count;
}

note_t *ysw_progression_get_notes(ysw_progression_t *progression)
{
    assert(progression);
    int chord_time = 0;
    int chord_count = ysw_progression_get_chord_count(progression);
    int note_count = ysw_progression_get_note_count(progression);
    note_t *notes = ysw_heap_allocate(sizeof(note_t) * note_count);
    note_t *note_p = notes;
    for (int i = 0; i < chord_count; i++) {
        ysw_slot_t *slot = ysw_progression_get_slot(progression, i);
        uint8_t chord_root = slot->degree;
        int chord_note_count = ysw_slot_get_chord_note_count(slot);
        for (int j = 0; j < chord_note_count; j++) {
            ysw_chord_note_t *chord_note = ysw_slot_get_chord_note(slot, j);
            note_p->time = chord_time + chord_note->time;
            note_p->duration = chord_note->duration;
            note_p->channel = 0;
            note_p->midi_note = to_note(progression->tonic, chord_root, chord_note->degree);
            note_p->velocity = chord_note->velocity;
            note_p->instrument = progression->instrument;
            ESP_LOGD(TAG, "time=%u, duration=%d, midi_note=%d, velocity=%d, instrument=%d", note_p->time, note_p->duration, note_p->midi_note, note_p->velocity, note_p->instrument);
            note_p++;
        }
        chord_time += ysw_slot_get_duration(slot);
    }
    return notes;
}

