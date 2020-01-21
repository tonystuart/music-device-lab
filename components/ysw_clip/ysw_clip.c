// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_clip.h"

#define TAG "ysw_clip"

ysw_clip_t *ysw_clip_create()
{
    ysw_clip_t *ysw_clip = ysw_heap_allocate(sizeof(ysw_clip_t));
    ysw_clip->chord_notes = ysw_array_create(4);
    ysw_clip->chords = ysw_array_create(8);
    ysw_clip->instrument = 0;
    ysw_clip->percent_tempo = 100;
    ysw_clip->tonic = 60;
    ysw_clip->measure_duration = 2000;
    ESP_LOGD(TAG, "create sequence=%p", ysw_clip);
    return ysw_clip;
}

int ysw_clip_add_chord_note(ysw_clip_t *s, ysw_chord_note_t *chord_note)
{
    return ysw_array_push(s->chord_notes, chord_note);
}

int ysw_clip_add_chord(ysw_clip_t *s, ysw_chord_t chord)
{
    return ysw_array_push(s->chords, (void*)chord);
}

void ysw_clip_free(ysw_clip_t *ysw_clip)
{
    ESP_LOGD(TAG, "free sequence=%p, chord_notes=%d, chords=%d", ysw_clip, ysw_array_get_count(ysw_clip->chord_notes), ysw_array_get_count(ysw_clip->chords));
    ysw_heap_free(ysw_clip->chord_notes);
    ysw_heap_free(ysw_clip->chords);
    ysw_heap_free(ysw_clip);
}

ysw_chord_note_t *ysw_chord_note_create(uint8_t degree, uint8_t velocity, uint32_t time, uint32_t duration)
{
    ESP_LOGD(TAG, "chord_note_crate degree=%u, velocity=%u, time=%u, duration=%u", degree, velocity, time, duration);
    ysw_chord_note_t *ysw_chord_note = ysw_heap_allocate(sizeof(ysw_chord_note_t));
    ysw_chord_note->time = time;
    ysw_chord_note->duration = duration;
    ysw_chord_note->degree = degree;
    ysw_chord_note->velocity = velocity;
    return ysw_chord_note;
}

void ysw_chord_note_free(ysw_chord_note_t *ysw_chord_note)
{
    ESP_LOGD(TAG, "free chord_note=%p, time=%u, degree=%u, chord_note=%u, velocity=%u", ysw_chord_note, ysw_chord_note->time, ysw_chord_note->degree, ysw_chord_note->degree, ysw_chord_note->velocity);
    ysw_heap_free(ysw_chord_note);
}

void ysw_clip_set_instrument(ysw_clip_t *sequence, uint8_t instrument)
{
    sequence->instrument = instrument;
}

void ysw_clip_set_percent_tempo(ysw_clip_t *sequence, uint8_t percent_tempo)
{
    sequence->percent_tempo = percent_tempo;
}

void ysw_clip_set_measure_duration(ysw_clip_t *sequence, uint32_t measure_duration)
{
    sequence->measure_duration = measure_duration;
}

note_t *ysw_clip_get_notes(ysw_clip_t *sequence)
{
    int measure_time = 0;
    int chord_count = ysw_array_get_count(sequence->chords);
    int chord_note_count = ysw_array_get_count(sequence->chord_notes);
    int note_count = chord_count * chord_note_count;
    note_t *notes = ysw_heap_allocate(sizeof(note_t) * note_count);
    note_t *note_p = notes;
    for (int i = 0; i < chord_count; i++) {
        uint8_t chord_root = (ysw_chord_t)ysw_array_get(sequence->chords, i);
        for (int j = 0; j < chord_note_count; j++) {
            ysw_chord_note_t *chord_note = ysw_array_get(sequence->chord_notes, j);
            note_p->time = measure_time + chord_note->time;
            note_p->duration = chord_note->duration;
            note_p->channel = 0;
            note_p->midi_note = sequence->tonic + chord_root + chord_note->degree;
            note_p->velocity = chord_note->velocity;
            note_p->instrument = sequence->instrument;
            note_p++;
        }
        measure_time += sequence->measure_duration;
    }
    return notes;
}

