// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_chord.h"

#include "assert.h"
#include "ysw_heap.h"

#define TAG "YSW_CHORD"

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
    ysw_heap_free(chord->name);
    ysw_heap_free(chord);
}

uint32_t ysw_chord_add_note(ysw_chord_t *chord, ysw_chord_note_t *chord_note)
{
    assert(chord);
    assert(chord_note);
    chord->duration = max(chord->duration, chord_note->time + chord_note->duration);
    uint32_t index = ysw_array_push(chord->chord_notes, chord_note);
    return index;
}

void ysw_chord_set_duration(ysw_chord_t *chord, uint32_t duration)
{
    assert(chord);
    chord->duration = duration;
}

ysw_chord_note_t *ysw_chord_note_create(int8_t degree, uint8_t velocity, uint32_t time, uint32_t duration)
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

void ysw_chord_dump(ysw_chord_t *chord, char *tag)
{
    ESP_LOGD(tag, "ysw_chord_dump chord=%p", chord);
    ESP_LOGD(tag, "name=%s", chord->name);
    ESP_LOGD(tag, "duration=%d", chord->duration);
    uint32_t note_count = ysw_chord_get_note_count(chord);
    ESP_LOGD(tag, "note_count=%d", note_count);
}

