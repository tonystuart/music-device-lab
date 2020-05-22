// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_cn.h"
#include "ysw_cs.h"

#include "assert.h"
#include "ysw_heap.h"

#define TAG "YSW_CS"

ysw_cs_t *ysw_cs_create(char *name, uint8_t instrument, uint8_t octave, ysw_mode_t mode, int8_t transposition, uint8_t tempo, uint8_t divisions)
{
    ysw_cs_t *cs = ysw_heap_allocate(sizeof(ysw_cs_t));
    cs->name = ysw_heap_strdup(name);
    cs->cn_array = ysw_array_create(8);
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
    uint32_t cn_count = ysw_cs_get_cn_count(old_cs);
    ysw_cs_t *new_cs = ysw_heap_allocate(sizeof(ysw_cs_t));
    new_cs->name = ysw_heap_strdup(old_cs->name);
    new_cs->cn_array = ysw_array_create(cn_count);
    new_cs->instrument = old_cs->instrument;
    new_cs->octave = old_cs->octave;
    new_cs->mode = old_cs->mode;
    new_cs->transposition = old_cs->transposition;
    new_cs->tempo = old_cs->tempo;
    new_cs->divisions = old_cs->divisions;
    for (int i = 0; i < cn_count; i++) {
        ysw_cn_t *old_cn = ysw_cs_get_cn(old_cs, i);
        ysw_cn_t *new_cn = ysw_cn_copy(old_cn);
        ysw_cs_add_cn(new_cs, new_cn);
    }
    return new_cs;
}

void ysw_cs_free(ysw_cs_t *cs)
{
    uint32_t cn_count = ysw_cs_get_cn_count(cs);
    for (int i = 0; i < cn_count; i++) {
        ysw_cn_t *cn = ysw_cs_get_cn(cs, i);
        ysw_cn_free(cn);
    }
    ysw_array_free(cs->cn_array);
    ysw_heap_free(cs->name);
    ysw_heap_free(cs);
}

uint32_t ysw_cs_add_cn(ysw_cs_t *cs, ysw_cn_t *cn)
{
    ysw_cn_normalize(cn);
    uint32_t index = ysw_array_push(cs->cn_array, cn);
    return index;
}

void ysw_cs_sort_cn_array(ysw_cs_t *cs)
{
    ysw_array_sort(cs->cn_array, ysw_cn_compare);
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

// cn_array must be in order produced by ysw_cs_sort_cn_array prior to call

note_t *ysw_cs_get_notes(ysw_cs_t *cs, uint32_t *note_count)
{
    int end_time = 0;
    int cn_count = ysw_cs_get_cn_count(cs);
    uint32_t max_note_count = cn_count;
    max_note_count += cs->divisions; // metronome ticks
    max_note_count += 1; // file-to-measure
    note_t *notes = ysw_heap_allocate(sizeof(note_t) * max_note_count);
    note_t *note_p = notes;
    uint8_t tonic = cs->octave * 12;
    uint8_t root = ysw_degree_intervals[0][cs->mode % 7];
    // TODO: Revisit whether root should be 1-based, factor out a 0-based function
    root++;
    uint32_t ticks_per_division = YSW_CS_DURATION / cs->divisions;
    uint32_t metronome_tick = 0;
    for (int j = 0; j < cn_count; j++) {
        ysw_cn_t *cn = ysw_cs_get_cn(cs, j);
        while (metronome_tick <= cn->start) {
            note_p->start = metronome_tick;
            note_p->duration = 0;
            note_p->channel = YSW_CS_CONTROL_CHANNEL;
            note_p->midi_note = YSW_CS_METRONOME_NOTE;
            note_p->velocity = 0;
            note_p->instrument = 0;
            note_p++;
            metronome_tick += ticks_per_division;
        }
        note_p->start = cn->start;
        note_p->duration = cn->duration;
        note_p->channel = YSW_CS_MUSIC_CHANNEL;
        note_p->midi_note = ysw_cn_to_midi_note(cn, tonic, root) + cs->transposition;
        note_p->velocity = cn->velocity;
        note_p->instrument = cs->instrument;
        //ESP_LOGD(TAG, "ysw_cs_get_notes start=%u, duration=%d, midi_note=%d, velocity=%d, instrument=%d", note_p->start, note_p->duration, note_p->midi_note, note_p->velocity, note_p->instrument);
        end_time = note_p->start + note_p->duration;
        note_p++;
    }
    uint32_t fill_to_measure = YSW_CS_DURATION - end_time;
    if (fill_to_measure) {
        note_p->start = end_time;
        note_p->duration = fill_to_measure;
        note_p->channel = YSW_CS_CONTROL_CHANNEL;
        note_p->midi_note = YSW_CS_FILL_TO_MEASURE_NOTE;
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
    uint32_t cn_count = ysw_cs_get_cn_count(cs);
    ESP_LOGD(tag, "cn_count=%d", cn_count);
}

