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
    hp->pss = ysw_array_create(8);
    hp->instrument = instrument;
    hp->octave = octave;
    hp->mode = mode;
    hp->transposition = transposition;
    hp->tempo = tempo;
    return hp;
}

ysw_hp_t *ysw_hp_copy(ysw_hp_t *old_hp)
{
    uint32_t ps_count = ysw_hp_get_ps_count(old_hp);
    ysw_hp_t *new_hp = ysw_heap_allocate(sizeof(ysw_hp_t));
    new_hp->name = ysw_heap_strdup(old_hp->name);
    new_hp->pss = ysw_array_create(ps_count);
    new_hp->instrument = old_hp->instrument;
    new_hp->octave = old_hp->octave;
    new_hp->mode = old_hp->mode;
    new_hp->transposition = old_hp->transposition;
    new_hp->tempo = old_hp->tempo;
    for (int i = 0; i < ps_count; i++) {
        ysw_ps_t *old_ps = ysw_hp_get_ps(old_hp, i);
        ysw_ps_t *new_ps = ysw_ps_copy(old_ps);
        ysw_hp_add_ps(new_hp, new_ps);
    }
    return new_hp;
}

void ysw_hp_free(ysw_hp_t *hp)
{
    assert(hp);
    ESP_LOGD(TAG, "ysw_hp_free name=%s", hp->name);
    int cs_count = ysw_array_get_count(hp->pss);
    for (int i = 0; i < cs_count; i++) {
        ysw_ps_t *ps = ysw_array_get(hp->pss, i);
        ysw_ps_free(ps);
    }
    ysw_array_free(hp->pss);
    ysw_heap_free(hp->name);
    ysw_heap_free(hp);
}

void ysw_hp_set_name(ysw_hp_t *hp, const char* name)
{
    hp->name = ysw_heap_string_reallocate(hp->name, name);
}

ysw_ps_t *ysw_ps_create(ysw_cs_t *cs, uint8_t degree, uint8_t flags)
{
    assert(cs);
    ysw_ps_t *ps = ysw_heap_allocate(sizeof(ysw_ps_t));
    ps->degree = degree;
    ps->cs = cs;
    ps->flags = flags;
    return ps;
}

int ysw_hp_add_ps(ysw_hp_t *hp, ysw_ps_t *ps)
{
    return ysw_array_push(hp->pss, ps);
}

void ysw_ps_free(ysw_ps_t *ps)
{
    ysw_heap_free(ps);
}

int ysw_hp_add_cs(ysw_hp_t *hp, uint8_t degree, ysw_cs_t *cs, uint8_t flags)
{
    assert(hp);
    assert(degree < YSW_MIDI_MAX);
    assert(cs);
    ysw_ps_t *ps = ysw_ps_create(cs, degree, flags);
    int index = ysw_hp_add_ps(hp, ps);
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
    uint32_t cs_count = ysw_array_get_count(hp->pss);
    ESP_LOGD(tag, "cs_count=%d", cs_count);
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_ps_t *ps = ysw_array_get(hp->pss, i);
        ESP_LOGD(tag, "cs index=%d, degree=%d", i, ps->degree);
        ysw_cs_dump(ps->cs, tag);
    }
}

int32_t ysw_hp_get_ps_index(ysw_hp_t *hp, ysw_ps_t *ps)
{
    return ysw_array_find(hp->pss, ps);
}

uint32_t ysw_hp_get_pss_to_next_measure(ysw_hp_t *hp, uint32_t ps_index)
{
    bool found = false;
    uint32_t pss_to_next_measure = 0;
    uint32_t ps_count = ysw_hp_get_ps_count(hp);
    for (uint32_t i = ps_index + 1; i < ps_count && !found; i++) {
        pss_to_next_measure++;
        ysw_ps_t *ps = ysw_hp_get_ps(hp, i);
        if (ysw_ps_is_new_measure(ps)) {
            found = true;
        }
    }
    return pss_to_next_measure;
}

uint32_t ysw_hp_get_measure_count(ysw_hp_t *hp)
{
    uint32_t measure_count = 0;
    uint32_t ps_count = ysw_hp_get_ps_count(hp);
    for (uint32_t i = 0; i < ps_count; i++) {
        ysw_ps_t *ps = ysw_hp_get_ps(hp, i);
        if (!i || ysw_ps_is_new_measure(ps)) {
            measure_count++;
        }
    }
    return measure_count;
}

uint32_t ysw_hp_get_pss_in_measures(ysw_hp_t *hp, uint8_t pss_in_measures[], uint32_t size)
{
    int32_t measure_index = -1;
    uint32_t ps_count = ysw_hp_get_ps_count(hp);
    assert(size >= ps_count);
    memset(pss_in_measures, 0, size);
    for (uint32_t i = 0; i < ps_count; i++) {
        ysw_ps_t *ps = ysw_hp_get_ps(hp, i);
        if (!i || ysw_ps_is_new_measure(ps)) {
            measure_index++;
        }
        pss_in_measures[measure_index]++;
    }
    return measure_index + 1;
}

static uint32_t ysw_hp_get_sn_count(ysw_hp_t *hp)
{
    assert(hp);
    uint32_t sn_count = 0;
    int ps_count = ysw_hp_get_ps_count(hp);
    for (int i = 0; i < ps_count; i++) {
        ysw_cs_t *cs = ysw_hp_get_cs(hp, i);
        sn_count += ysw_cs_get_sn_count(cs);
    }
    return sn_count;
}

ysw_note_t *ysw_hp_get_notes(ysw_hp_t *hp, uint32_t *note_count)
{
    assert(hp);
    assert(note_count);

    int ps_count = ysw_hp_get_ps_count(hp);
    uint8_t pss_in_measures[ps_count];
    ysw_hp_get_pss_in_measures(hp, pss_in_measures, ps_count);

    uint32_t sn_count = ysw_hp_get_sn_count(hp);
    uint32_t max_note_count = sn_count;
    max_note_count += ps_count; // metronome ticks
    max_note_count += 1; // fill-to-measure
    ysw_note_t *notes = ysw_heap_allocate(sizeof(ysw_note_t) * max_note_count);
    ysw_note_t *note_p = notes;

    int cs_time = 0;
    uint32_t measure = 0;
    double time_scaler = 1;
    uint8_t tonic = (hp->octave * 12) + ysw_degree_intervals[0][hp->mode % 7];

    for (int i = 0; i < ps_count; i++) {

        ysw_ps_t *ps = ysw_hp_get_ps(hp, i);
        if (!i || ysw_ps_is_new_measure(ps)) {
            time_scaler = 1.0 / pss_in_measures[measure];
            measure++;
        }

        note_p->start = cs_time;
        note_p->duration = 0;
        note_p->channel = YSW_MIDI_STATUS_CHANNEL;
        note_p->midi_note = YSW_MIDI_STATUS_NOTE;
        note_p->velocity = i; // TODO: find a legit way to pass this to those who need it
        note_p->instrument = 0;
        note_p++;

        uint8_t cs_root = ps->degree;
        int sn_count = ysw_ps_get_sn_count(ps);
        for (int j = 0; j < sn_count; j++) {
            ysw_sn_t *sn = ysw_ps_get_sn(ps, j);
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

