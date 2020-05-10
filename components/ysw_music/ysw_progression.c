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
    progression->steps = ysw_array_create(8);
    progression->instrument = instrument;
    progression->tonic = tonic;
    return progression;
}

void ysw_progression_free(ysw_progression_t *progression)
{
    assert(progression);
    ESP_LOGD(TAG, "ysw_progression_free name=%s", progression->name);
    int cs_count = ysw_array_get_count(progression->steps);
    for (int i = 0; i < cs_count; i++) {
        ysw_step_t *step = ysw_array_get(progression->steps, i);
        ysw_heap_free(step);
    }
    ysw_array_free(progression->steps);
    ysw_heap_free(progression->name);
    ysw_heap_free(progression);
}

int ysw_progression_add_cs(ysw_progression_t *progression, uint8_t degree, ysw_cs_t *cs)
{
    assert(progression);
    assert(degree < YSW_MIDI_MAX);
    assert(cs);
    ysw_step_t *step = ysw_heap_allocate(sizeof(ysw_step_t));
    step->degree = degree;
    step->cs = cs;
    int index = ysw_array_push(progression->steps, step);
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
    uint32_t cs_count = ysw_array_get_count(progression->steps);
    ESP_LOGD(tag, "cs_count=%d", cs_count);
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_step_t *step = ysw_array_get(progression->steps, i);
        ESP_LOGD(tag, "cs index=%d, degree=%d", i, step->degree);
        ysw_cs_dump(step->cs, tag);
    }
}

uint32_t ysw_progression_get_note_count(ysw_progression_t *progression)
{
    assert(progression);
    uint32_t note_count = 0;
    int cs_count = ysw_progression_get_cs_count(progression);
    for (int i = 0; i < cs_count; i++) {
        ESP_LOGD(TAG, "i=%d, cs_count=%d", i, cs_count);
        ysw_cs_t *cs = ysw_progression_get_cs(progression, i);
        int csn_count = ysw_array_get_count(cs->csn_array);
        note_count += csn_count;
    }
    return note_count;
}

note_t *ysw_progression_get_notes(ysw_progression_t *progression)
{
    assert(progression);
    int cs_time = 0;
    int cs_count = ysw_progression_get_cs_count(progression);
    int note_count = ysw_progression_get_note_count(progression);
    note_t *notes = ysw_heap_allocate(sizeof(note_t) * note_count);
    note_t *note_p = notes;
    for (int i = 0; i < cs_count; i++) {
        ysw_step_t *step = ysw_progression_get_step(progression, i);
        uint8_t cs_root = step->degree;
        int csn_count = ysw_step_get_csn_count(step);
        for (int j = 0; j < csn_count; j++) {
            ysw_csn_t *csn = ysw_step_get_csn(step, j);
            note_p->start = cs_time + csn->start;
            note_p->duration = csn->duration;
            note_p->channel = 0;
            note_p->midi_note = ysw_csn_to_midi_note(csn, progression->tonic, cs_root);
            note_p->velocity = csn->velocity;
            note_p->instrument = progression->instrument;
            ESP_LOGD(TAG, "start=%u, duration=%d, midi_note=%d, velocity=%d, instrument=%d", note_p->start, note_p->duration, note_p->midi_note, note_p->velocity, note_p->instrument);
            note_p++;
        }
        cs_time += ysw_step_get_duration(step);
    }
    return notes;
}

