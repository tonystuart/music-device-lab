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
#include "ysw_ticks.h"
#include "ysw_degree.h"

#define TAG "YSW_PROGRESSION"

ysw_progression_t *ysw_progression_create(char *name, uint8_t tonic, uint8_t instrument)
{
    ESP_LOGD(TAG, "ysw_progression_create name=%s", name);
    ysw_progression_t *progression = ysw_heap_allocate(sizeof(ysw_progression_t));
    progression->name = ysw_heap_strdup(name);
    progression->slots = ysw_array_create(8);
    progression->instrument = instrument;
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

void ysw_progression_dump(ysw_progression_t *progression, char *tag)
{
    ESP_LOGD(tag, "ysw_progression_dump progression=%p", progression);
    ESP_LOGD(tag, "name=%s", progression->name);
    ESP_LOGD(tag, "tonic=%d", progression->tonic);
    ESP_LOGD(tag, "instrument=%d", progression->instrument);
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
            note_p->start = chord_time + chord_note->start;
            note_p->duration = chord_note->duration;
            note_p->channel = 0;
            note_p->midi_note = ysw_degree_to_note(progression->tonic, chord_root, chord_note->degree, chord_note->accidental);
            note_p->velocity = chord_note->velocity;
            note_p->instrument = progression->instrument;
            ESP_LOGD(TAG, "start=%u, duration=%d, midi_note=%d, velocity=%d, instrument=%d", note_p->start, note_p->duration, note_p->midi_note, note_p->velocity, note_p->instrument);
            note_p++;
        }
        chord_time += ysw_slot_get_duration(slot);
    }
    return notes;
}

