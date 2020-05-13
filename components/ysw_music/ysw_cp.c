// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// TODO: Change type of MIDI 7 bit quantities to int8_t

#include "ysw_cp.h"

#include "assert.h"
#include "ysw_heap.h"
#include "ysw_ticks.h"
#include "ysw_degree.h"

#define TAG "YSW_PROGRESSION"

ysw_cp_t *ysw_cp_create(char *name, uint8_t instrument, uint8_t octave, ysw_mode_t mode, int8_t transposition, uint8_t tempo, ysw_time_t time)
{
    ESP_LOGD(TAG, "ysw_cp_create name=%s", name);
    ysw_cp_t *cp = ysw_heap_allocate(sizeof(ysw_cp_t));
    cp->name = ysw_heap_strdup(name);
    cp->steps = ysw_array_create(8);
    cp->instrument = instrument;
    cp->tonic = (octave * 12) + mode; // TODO: deprecate in favor of octave and mode
    cp->octave = octave;
    cp->mode = mode;
    cp->transposition = transposition;
    cp->tempo = tempo;
    cp->time = time;
    return cp;
}

void ysw_cp_free(ysw_cp_t *cp)
{
    assert(cp);
    ESP_LOGD(TAG, "ysw_cp_free name=%s", cp->name);
    int cs_count = ysw_array_get_count(cp->steps);
    for (int i = 0; i < cs_count; i++) {
        ysw_step_t *step = ysw_array_get(cp->steps, i);
        ysw_step_free(step);
    }
    ysw_array_free(cp->steps);
    ysw_heap_free(cp->name);
    ysw_heap_free(cp);
}

void ysw_cp_set_name(ysw_cp_t *cp, const char* name)
{
    cp->name = ysw_heap_string_reallocate(cp->name, name);
}

ysw_step_t *ysw_step_create(ysw_cs_t *cs, uint8_t degree)
{
    assert(cs);
    ysw_step_t *step = ysw_heap_allocate(sizeof(ysw_step_t));
    step->degree = degree;
    step->cs = cs;
    return step;
}

int ysw_cp_add_step(ysw_cp_t *cp, ysw_step_t *step)
{
    return ysw_array_push(cp->steps, step);
}

void ysw_step_free(ysw_step_t *step)
{
    ysw_heap_free(step);
}

// TODO: reorder degree, cs in parameter list

int ysw_cp_add_cs(ysw_cp_t *cp, uint8_t degree, ysw_cs_t *cs)
{
    assert(cp);
    assert(degree < YSW_MIDI_MAX);
    assert(cs);
    ysw_step_t *step = ysw_step_create(cs, degree);
    int index = ysw_cp_add_step(cp, step);
    return index;
}

void ysw_cp_set_tonic(ysw_cp_t *cp, uint8_t tonic)
{
    assert(cp);
    assert(tonic < YSW_MIDI_MAX);
    cp->tonic = tonic;
}

void ysw_cp_set_instrument(ysw_cp_t *cp, uint8_t instrument)
{
    assert(cp);
    assert(instrument < YSW_MIDI_MAX);
    cp->instrument = instrument;
}

void ysw_cp_dump(ysw_cp_t *cp, char *tag)
{
    ESP_LOGD(tag, "ysw_cp_dump cp=%p", cp);
    ESP_LOGD(tag, "name=%s", cp->name);
    ESP_LOGD(tag, "tonic=%d", cp->tonic);
    ESP_LOGD(tag, "instrument=%d", cp->instrument);
    uint32_t cs_count = ysw_array_get_count(cp->steps);
    ESP_LOGD(tag, "cs_count=%d", cs_count);
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_step_t *step = ysw_array_get(cp->steps, i);
        ESP_LOGD(tag, "cs index=%d, degree=%d", i, step->degree);
        ysw_cs_dump(step->cs, tag);
    }
}

// TODO: consider merging into ysw_cp_get_notes

static uint32_t ysw_cp_get_cn_count(ysw_cp_t *cp)
{
    assert(cp);
    uint32_t cn_count = 0;
    int cs_count = ysw_cp_get_step_count(cp);
    for (int i = 0; i < cs_count; i++) {
        ESP_LOGD(TAG, "i=%d, cs_count=%d", i, cs_count);
        ysw_cs_t *cs = ysw_cp_get_cs(cp, i);
        cn_count += ysw_cs_get_cn_count(cs);
    }
    return cn_count;
}

note_t *ysw_cp_get_notes(ysw_cp_t *cp, uint32_t *note_count)
{
    assert(cp);
    assert(note_count);
    int cs_time = 0;
    int step_count = ysw_cp_get_step_count(cp);
    int cn_count = ysw_cp_get_cn_count(cp);
    note_t *notes = ysw_heap_allocate(sizeof(note_t) * (cn_count + 1)); // +1 for fill-to-measure
    note_t *note_p = notes;
    for (int i = 0; i < step_count; i++) {
        ysw_step_t *step = ysw_cp_get_step(cp, i);
        uint8_t cs_root = step->degree;
        int cn_count = ysw_step_get_cn_count(step);
        for (int j = 0; j < cn_count; j++) {
            ysw_cn_t *cn = ysw_step_get_cn(step, j);
            note_p->start = cs_time + cn->start;
            note_p->duration = cn->duration;
            note_p->channel = 0;
            note_p->midi_note = ysw_cn_to_midi_note(cn, cp->tonic, cs_root);
            note_p->velocity = cn->velocity;
            note_p->instrument = cp->instrument;
            ESP_LOGD(TAG, "start=%u, duration=%d, midi_note=%d, velocity=%d, instrument=%d", note_p->start, note_p->duration, note_p->midi_note, note_p->velocity, note_p->instrument);
            note_p++;
        }
        cs_time += ysw_step_get_duration(step);
    }
    *note_count = note_p - notes;
    return notes;
}

