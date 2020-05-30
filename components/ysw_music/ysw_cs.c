// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_cs.h"
#include "ysw_heap.h"
#include "ysw_sn.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_CS"

ysw_cs_t *ysw_cs_create(char *name, uint8_t instrument, uint8_t octave, ysw_mode_t mode, int8_t transposition, uint8_t tempo, uint8_t divisions)
{
    ysw_cs_t *cs = ysw_heap_allocate(sizeof(ysw_cs_t));
    cs->name = ysw_heap_strdup(name);
    cs->sn_array = ysw_array_create(8);
    cs->instrument = instrument;
    cs->octave = octave;
    cs->mode = mode;
    cs->transposition = transposition;
    cs->tempo = tempo;
    cs->divisions = divisions;
    return cs;
}

ysw_cs_t *ysw_cs_copy(ysw_cs_t *old_cs)
{
    uint32_t sn_count = ysw_cs_get_sn_count(old_cs);
    ysw_cs_t *new_cs = ysw_heap_allocate(sizeof(ysw_cs_t));
    new_cs->name = ysw_heap_strdup(old_cs->name);
    new_cs->sn_array = ysw_array_create(sn_count);
    new_cs->instrument = old_cs->instrument;
    new_cs->octave = old_cs->octave;
    new_cs->mode = old_cs->mode;
    new_cs->transposition = old_cs->transposition;
    new_cs->tempo = old_cs->tempo;
    new_cs->divisions = old_cs->divisions;
    for (int i = 0; i < sn_count; i++) {
        ysw_sn_t *old_sn = ysw_cs_get_sn(old_cs, i);
        ysw_sn_t *new_sn = ysw_sn_copy(old_sn);
        ysw_cs_add_sn(new_cs, new_sn);
    }
    return new_cs;
}

void ysw_cs_free(ysw_cs_t *cs)
{
    uint32_t sn_count = ysw_cs_get_sn_count(cs);
    for (int i = 0; i < sn_count; i++) {
        ysw_sn_t *sn = ysw_cs_get_sn(cs, i);
        ysw_sn_free(sn);
    }
    ysw_array_free(cs->sn_array);
    ysw_heap_free(cs->name);
    ysw_heap_free(cs);
}

uint32_t ysw_cs_add_sn(ysw_cs_t *cs, ysw_sn_t *sn)
{
    uint32_t index = ysw_array_push(cs->sn_array, sn);
    return index;
}

void ysw_cs_sort_sn_array(ysw_cs_t *cs)
{
    ysw_array_sort(cs->sn_array, ysw_sn_compare);
}

void ysw_cs_set_name(ysw_cs_t *cs, const char* name)
{
    cs->name = ysw_heap_string_reallocate(cs->name, name);
}

void ysw_cs_set_instrument(ysw_cs_t *cs, uint8_t instrument)
{
    cs->instrument = instrument;
}

const char* ysw_cs_get_name(ysw_cs_t *cs)
{
    return cs->name;
}

uint8_t ysw_cs_get_instrument(ysw_cs_t *cs)
{
    return cs->instrument;
}

// sn_array must be in order produced by ysw_cs_sort_sn_array prior to call

ysw_note_t *ysw_cs_get_notes(ysw_cs_t *cs, uint32_t *note_count)
{
    int end_time = 0;
    int sn_count = ysw_cs_get_sn_count(cs);
    uint32_t max_note_count = sn_count;
    max_note_count += cs->divisions; // metronome ticks
    max_note_count += 1; // fill-to-measure
    ysw_note_t *notes = ysw_heap_allocate(sizeof(ysw_note_t) * max_note_count);
    ysw_note_t *note_p = notes;
    uint8_t tonic = cs->octave * 12;
    uint8_t root = ysw_degree_intervals[0][cs->mode % 7];
    // TODO: Revisit whether root should be 1-based, factor out a 0-based function
    root++;
    uint32_t ticks_per_division = YSW_CS_DURATION / cs->divisions;
    uint32_t metronome_tick = 0;
    for (int j = 0; j < sn_count; j++) {
        ysw_sn_t *sn = ysw_cs_get_sn(cs, j);
        while (metronome_tick <= sn->start) {
            note_p->start = metronome_tick;
            note_p->duration = 0;
            note_p->channel = YSW_CS_META_CHANNEL;
            note_p->midi_note = YSW_CS_METRO;
            note_p->velocity = 0;
            note_p->instrument = 0;
            note_p++;
            metronome_tick += ticks_per_division;
        }
        note_p->start = sn->start;
        note_p->duration = sn->duration;
        note_p->channel = YSW_CS_MUSIC_CHANNEL;
        note_p->midi_note = ysw_sn_to_midi_note(sn, tonic, root) + cs->transposition;
        note_p->velocity = sn->velocity;
        note_p->instrument = cs->instrument;
        //ESP_LOGD(TAG, "ysw_cs_get_notes start=%u, duration=%d, midi_note=%d, velocity=%d, instrument=%d", note_p->start, note_p->duration, note_p->midi_note, note_p->velocity, note_p->instrument);
        end_time = note_p->start + note_p->duration;
        note_p++;
    }
    uint32_t fill_to_measure = YSW_CS_DURATION - end_time;
    if (fill_to_measure) {
        note_p->start = end_time;
        note_p->duration = fill_to_measure;
        note_p->channel = YSW_CS_META_CHANNEL;
        note_p->midi_note = YSW_CS_META_FILL;
        note_p->velocity = 0;
        note_p->instrument = 0;
        note_p++;
    }
    *note_count = note_p - notes;
    return notes;
}

void ysw_cs_dump(ysw_cs_t *cs, char *tag)
{
    ESP_LOGD(tag, "ysw_cs_dump cs=%p", cs);
    ESP_LOGD(tag, "name=%s", cs->name);
    uint32_t sn_count = ysw_cs_get_sn_count(cs);
    ESP_LOGD(tag, "sn_count=%d", sn_count);
}

