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

#define TAG "YSW_CP"

ysw_cp_t *ysw_cp_create(char *name, uint8_t instrument, uint8_t octave, ysw_mode_t mode, int8_t transposition, uint8_t tempo)
{
    ESP_LOGD(TAG, "ysw_cp_create name=%s", name);
    ysw_cp_t *cp = ysw_heap_allocate(sizeof(ysw_cp_t));
    cp->name = ysw_heap_strdup(name);
    cp->steps = ysw_array_create(8);
    cp->instrument = instrument;
    cp->octave = octave;
    cp->mode = mode;
    cp->transposition = transposition;
    cp->tempo = tempo;
    return cp;
}

ysw_cp_t *ysw_cp_copy(ysw_cp_t *old_cp)
{
    uint32_t step_count = ysw_cp_get_step_count(old_cp);
    ysw_cp_t *new_cp = ysw_heap_allocate(sizeof(ysw_cp_t));
    new_cp->name = ysw_heap_strdup(old_cp->name);
    new_cp->steps = ysw_array_create(step_count);
    new_cp->instrument = old_cp->instrument;
    new_cp->octave = old_cp->octave;
    new_cp->mode = old_cp->mode;
    new_cp->transposition = old_cp->transposition;
    new_cp->tempo = old_cp->tempo;
    for (int i = 0; i < step_count; i++) {
        ysw_step_t *old_step = ysw_cp_get_step(old_cp, i);
        ysw_step_t *new_step = ysw_step_copy(old_step);
        ysw_cp_add_step(new_cp, new_step);
    }
    return new_cp;
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

ysw_step_t *ysw_step_create(ysw_cs_t *cs, uint8_t degree, uint8_t flags)
{
    assert(cs);
    ysw_step_t *step = ysw_heap_allocate(sizeof(ysw_step_t));
    step->degree = degree;
    step->cs = cs;
    step->flags = flags;
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

int ysw_cp_add_cs(ysw_cp_t *cp, uint8_t degree, ysw_cs_t *cs, uint8_t flags)
{
    assert(cp);
    assert(degree < YSW_MIDI_MAX);
    assert(cs);
    ysw_step_t *step = ysw_step_create(cs, degree, flags);
    int index = ysw_cp_add_step(cp, step);
    return index;
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
    ESP_LOGD(tag, "instrument=%d", cp->instrument);
    uint32_t cs_count = ysw_array_get_count(cp->steps);
    ESP_LOGD(tag, "cs_count=%d", cs_count);
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_step_t *step = ysw_array_get(cp->steps, i);
        ESP_LOGD(tag, "cs index=%d, degree=%d", i, step->degree);
        ysw_cs_dump(step->cs, tag);
    }
}

int32_t ysw_cp_get_step_index(ysw_cp_t *cp, ysw_step_t *target_step)
{
    uint32_t step_count = ysw_cp_get_step_count(cp);
    for (uint32_t i = 0; i < step_count; i++) {
        ysw_step_t *step = ysw_cp_get_step(cp, i);
        if (step == target_step) {
            return i;
        }
    }
    return -1;
}

uint32_t ysw_cp_get_steps_to_next_measure(ysw_cp_t *cp, uint32_t step_index)
{
    bool found = false;
    uint32_t steps_to_next_measure = 0;
    uint32_t step_count = ysw_cp_get_step_count(cp);
    for (uint32_t i = step_index + 1; i < step_count && !found; i++) {
        steps_to_next_measure++;
        ysw_step_t *step = ysw_cp_get_step(cp, i);
        if (step->flags & YSW_STEP_NEW_MEASURE) {
            found = true;
        }
    }
    return steps_to_next_measure;
}

uint32_t ysw_cp_get_measure_count(ysw_cp_t *cp)
{
    uint32_t measure_count = 0;
    uint32_t step_count = ysw_cp_get_step_count(cp);
    for (uint32_t i = 0; i < step_count; i++) {
        ysw_step_t *step = ysw_cp_get_step(cp, i);
        if (!i || (step->flags & YSW_STEP_NEW_MEASURE)) {
            measure_count++;
        }
    }
    return measure_count;
}

uint32_t ysw_cp_get_steps_in_measures(ysw_cp_t *cp, uint8_t steps_in_measures[], uint32_t size)
{
    int32_t measure_index = -1;
    uint32_t step_count = ysw_cp_get_step_count(cp);
    assert(size >= step_count);
    memset(steps_in_measures, 0, size);
    for (uint32_t i = 0; i < step_count; i++) {
        ysw_step_t *step = ysw_cp_get_step(cp, i);
        if (!i || (step->flags & YSW_STEP_NEW_MEASURE)) {
            measure_index++;
        }
        steps_in_measures[measure_index]++;
    }
    return measure_index + 1;
}

static uint32_t ysw_cp_get_cn_count(ysw_cp_t *cp)
{
    assert(cp);
    uint32_t cn_count = 0;
    int step_count = ysw_cp_get_step_count(cp);
    for (int i = 0; i < step_count; i++) {
        ysw_cs_t *cs = ysw_cp_get_cs(cp, i);
        cn_count += ysw_cs_get_cn_count(cs);
    }
    return cn_count;
}

note_t *ysw_cp_get_notes(ysw_cp_t *cp, uint32_t *note_count)
{
    assert(cp);
    assert(note_count);

    int step_count = ysw_cp_get_step_count(cp);
    uint8_t steps_in_measures[step_count];
    ysw_cp_get_steps_in_measures(cp, steps_in_measures, step_count);

    uint32_t cn_count = ysw_cp_get_cn_count(cp);
    uint32_t max_note_count = cn_count;
    max_note_count += step_count; // metronome ticks
    max_note_count += 1; // fill-to-measure
    note_t *notes = ysw_heap_allocate(sizeof(note_t) * max_note_count);
    note_t *note_p = notes;

    int cs_time = 0;
    uint32_t measure = 0;
    double time_scaler = 1;
    uint8_t tonic = (cp->octave * 12) + ysw_degree_intervals[0][cp->mode % 7];

    for (int i = 0; i < step_count; i++) {

        ysw_step_t *step = ysw_cp_get_step(cp, i);
        if (!i || (step->flags & YSW_STEP_NEW_MEASURE)) {
            time_scaler = 1.0 / steps_in_measures[measure];
            measure++;
        }

        note_p->start = cs_time;
        note_p->duration = 0;
        note_p->channel = YSW_CS_META_CHANNEL;
        note_p->midi_note = YSW_CP_METRO;
        note_p->velocity = i; // TODO: find a legit way to pass this to those who need it
        note_p->instrument = 0;
        note_p++;

        uint8_t cs_root = step->degree;
        int cn_count = ysw_step_get_cn_count(step);
        for (int j = 0; j < cn_count; j++) {
            ysw_cn_t *cn = ysw_step_get_cn(step, j);
            uint32_t note_start = cs_time + (cn->start * time_scaler);

            note_p->start = note_start;
            note_p->duration = cn->duration * time_scaler;
            note_p->channel = YSW_CS_MUSIC_CHANNEL;
            note_p->midi_note = ysw_cn_to_midi_note(cn, tonic, cs_root);
            note_p->velocity = cn->velocity;
            note_p->instrument = cp->instrument;
            //ESP_LOGD(TAG, "note start=%u, duration=%d, midi_note=%d, velocity=%d, instrument=%d", note_p->start, note_p->duration, note_p->midi_note, note_p->velocity, note_p->instrument);
            note_p++;
        }
        cs_time += YSW_CS_DURATION * time_scaler;
    }
    *note_count = note_p - notes;
    return notes;
}

