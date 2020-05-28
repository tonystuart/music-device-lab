// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// TODO: Change type of MIDI 7 bit quantities to int8_t

#include "ysw_hp.h"
#include "ysw_heap.h"
#include "ysw_ticks.h"
#include "ysw_degree.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_HP"

ysw_hp_t *ysw_hp_create(char *name, uint8_t instrument, uint8_t octave, ysw_mode_t mode, int8_t transposition, uint8_t tempo)
{
    ESP_LOGD(TAG, "ysw_hp_create name=%s", name);
    ysw_hp_t *hp = ysw_heap_allocate(sizeof(ysw_hp_t));
    hp->name = ysw_heap_strdup(name);
    hp->steps = ysw_array_create(8);
    hp->instrument = instrument;
    hp->octave = octave;
    hp->mode = mode;
    hp->transposition = transposition;
    hp->tempo = tempo;
    return hp;
}

ysw_hp_t *ysw_hp_copy(ysw_hp_t *old_hp)
{
    uint32_t step_count = ysw_hp_get_step_count(old_hp);
    ysw_hp_t *new_hp = ysw_heap_allocate(sizeof(ysw_hp_t));
    new_hp->name = ysw_heap_strdup(old_hp->name);
    new_hp->steps = ysw_array_create(step_count);
    new_hp->instrument = old_hp->instrument;
    new_hp->octave = old_hp->octave;
    new_hp->mode = old_hp->mode;
    new_hp->transposition = old_hp->transposition;
    new_hp->tempo = old_hp->tempo;
    for (int i = 0; i < step_count; i++) {
        ysw_step_t *old_step = ysw_hp_get_step(old_hp, i);
        ysw_step_t *new_step = ysw_step_copy(old_step);
        ysw_hp_add_step(new_hp, new_step);
    }
    return new_hp;
}

void ysw_hp_free(ysw_hp_t *hp)
{
    assert(hp);
    ESP_LOGD(TAG, "ysw_hp_free name=%s", hp->name);
    int cs_count = ysw_array_get_count(hp->steps);
    for (int i = 0; i < cs_count; i++) {
        ysw_step_t *step = ysw_array_get(hp->steps, i);
        ysw_step_free(step);
    }
    ysw_array_free(hp->steps);
    ysw_heap_free(hp->name);
    ysw_heap_free(hp);
}

void ysw_hp_set_name(ysw_hp_t *hp, const char* name)
{
    hp->name = ysw_heap_string_reallocate(hp->name, name);
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

int ysw_hp_add_step(ysw_hp_t *hp, ysw_step_t *step)
{
    return ysw_array_push(hp->steps, step);
}

void ysw_step_free(ysw_step_t *step)
{
    ysw_heap_free(step);
}

int ysw_hp_add_cs(ysw_hp_t *hp, uint8_t degree, ysw_cs_t *cs, uint8_t flags)
{
    assert(hp);
    assert(degree < YSW_MIDI_MAX);
    assert(cs);
    ysw_step_t *step = ysw_step_create(cs, degree, flags);
    int index = ysw_hp_add_step(hp, step);
    return index;
}

void ysw_hp_set_instrument(ysw_hp_t *hp, uint8_t instrument)
{
    assert(hp);
    assert(instrument < YSW_MIDI_MAX);
    hp->instrument = instrument;
}

void ysw_hp_dump(ysw_hp_t *hp, char *tag)
{
    ESP_LOGD(tag, "ysw_hp_dump hp=%p", hp);
    ESP_LOGD(tag, "name=%s", hp->name);
    ESP_LOGD(tag, "instrument=%d", hp->instrument);
    uint32_t cs_count = ysw_array_get_count(hp->steps);
    ESP_LOGD(tag, "cs_count=%d", cs_count);
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_step_t *step = ysw_array_get(hp->steps, i);
        ESP_LOGD(tag, "cs index=%d, degree=%d", i, step->degree);
        ysw_cs_dump(step->cs, tag);
    }
}

int32_t ysw_hp_get_step_index(ysw_hp_t *hp, ysw_step_t *step)
{
    return ysw_array_find(hp->steps, step);
}

uint32_t ysw_hp_get_steps_to_next_measure(ysw_hp_t *hp, uint32_t step_index)
{
    bool found = false;
    uint32_t steps_to_next_measure = 0;
    uint32_t step_count = ysw_hp_get_step_count(hp);
    for (uint32_t i = step_index + 1; i < step_count && !found; i++) {
        steps_to_next_measure++;
        ysw_step_t *step = ysw_hp_get_step(hp, i);
        if (ysw_step_is_new_measure(step)) {
            found = true;
        }
    }
    return steps_to_next_measure;
}

uint32_t ysw_hp_get_measure_count(ysw_hp_t *hp)
{
    uint32_t measure_count = 0;
    uint32_t step_count = ysw_hp_get_step_count(hp);
    for (uint32_t i = 0; i < step_count; i++) {
        ysw_step_t *step = ysw_hp_get_step(hp, i);
        if (!i || ysw_step_is_new_measure(step)) {
            measure_count++;
        }
    }
    return measure_count;
}

uint32_t ysw_hp_get_steps_in_measures(ysw_hp_t *hp, uint8_t steps_in_measures[], uint32_t size)
{
    int32_t measure_index = -1;
    uint32_t step_count = ysw_hp_get_step_count(hp);
    assert(size >= step_count);
    memset(steps_in_measures, 0, size);
    for (uint32_t i = 0; i < step_count; i++) {
        ysw_step_t *step = ysw_hp_get_step(hp, i);
        if (!i || ysw_step_is_new_measure(step)) {
            measure_index++;
        }
        steps_in_measures[measure_index]++;
    }
    return measure_index + 1;
}

static uint32_t ysw_hp_get_sn_count(ysw_hp_t *hp)
{
    assert(hp);
    uint32_t sn_count = 0;
    int step_count = ysw_hp_get_step_count(hp);
    for (int i = 0; i < step_count; i++) {
        ysw_cs_t *cs = ysw_hp_get_cs(hp, i);
        sn_count += ysw_cs_get_sn_count(cs);
    }
    return sn_count;
}

ysw_note_t *ysw_hp_get_notes(ysw_hp_t *hp, uint32_t *note_count)
{
    assert(hp);
    assert(note_count);

    int step_count = ysw_hp_get_step_count(hp);
    uint8_t steps_in_measures[step_count];
    ysw_hp_get_steps_in_measures(hp, steps_in_measures, step_count);

    uint32_t sn_count = ysw_hp_get_sn_count(hp);
    uint32_t max_note_count = sn_count;
    max_note_count += step_count; // metronome ticks
    max_note_count += 1; // fill-to-measure
    ysw_note_t *notes = ysw_heap_allocate(sizeof(ysw_note_t) * max_note_count);
    ysw_note_t *note_p = notes;

    int cs_time = 0;
    uint32_t measure = 0;
    double time_scaler = 1;
    uint8_t tonic = (hp->octave * 12) + ysw_degree_intervals[0][hp->mode % 7];

    for (int i = 0; i < step_count; i++) {

        ysw_step_t *step = ysw_hp_get_step(hp, i);
        if (!i || ysw_step_is_new_measure(step)) {
            time_scaler = 1.0 / steps_in_measures[measure];
            measure++;
        }

        note_p->start = cs_time;
        note_p->duration = 0;
        note_p->channel = YSW_CS_META_CHANNEL;
        note_p->midi_note = YSW_HP_METRO;
        note_p->velocity = i; // TODO: find a legit way to pass this to those who need it
        note_p->instrument = 0;
        note_p++;

        uint8_t cs_root = step->degree;
        int sn_count = ysw_step_get_sn_count(step);
        for (int j = 0; j < sn_count; j++) {
            ysw_sn_t *sn = ysw_step_get_sn(step, j);
            uint32_t note_start = cs_time + (sn->start * time_scaler);

            note_p->start = note_start;
            note_p->duration = sn->duration * time_scaler;
            note_p->channel = YSW_CS_MUSIC_CHANNEL;
            note_p->midi_note = ysw_sn_to_midi_note(sn, tonic, cs_root);
            note_p->velocity = sn->velocity;
            note_p->instrument = hp->instrument;
            //ESP_LOGD(TAG, "note start=%u, duration=%d, midi_note=%d, velocity=%d, instrument=%d", note_p->start, note_p->duration, note_p->midi_note, note_p->velocity, note_p->instrument);
            note_p++;
        }
        cs_time += YSW_CS_DURATION * time_scaler;
    }
    *note_count = note_p - notes;
    return notes;
}

