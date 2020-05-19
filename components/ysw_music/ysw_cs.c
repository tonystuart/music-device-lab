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

ysw_cs_t *ysw_cs_create(char *name, uint8_t instrument, uint8_t octave, ysw_mode_t mode, int8_t transposition, uint8_t tempo, ysw_time_t time)
{
    ysw_cs_t *cs = ysw_heap_allocate(sizeof(ysw_cs_t));
    cs->name = ysw_heap_strdup(name);
    cs->cn_array = ysw_array_create(8);
    cs->instrument = instrument;
    cs->octave = octave;
    cs->mode = mode;
    cs->transposition = transposition;
    cs->tempo = tempo;
    cs->time = time;
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
    new_cs->time = old_cs->time;
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

static inline uint32_t round_tick(uint32_t value)
{
    return ((value + YSW_CSN_TICK_INCREMENT - 1) / YSW_CSN_TICK_INCREMENT) * YSW_CSN_TICK_INCREMENT;
}

void ysw_cs_normalize_cn(ysw_cs_t *cs, ysw_cn_t *cn)
{
    uint32_t cs_duration = ysw_cs_get_duration(cs);
    if (cn->start > cs_duration - YSW_CSN_MIN_DURATION) {
        cn->start = cs_duration - YSW_CSN_MIN_DURATION;
    }
    if (cn->duration < YSW_CSN_MIN_DURATION) {
        cn->duration = YSW_CSN_MIN_DURATION;
    }
    if (cn->start + cn->duration > cs_duration) {
        cn->duration = cs_duration - cn->start;
    }
    if (cn->degree < YSW_CSN_MIN_DEGREE) {
        cn->degree = YSW_CSN_MIN_DEGREE;
    } else if (cn->degree > YSW_CSN_MAX_DEGREE) {
        cn->degree = YSW_CSN_MAX_DEGREE;
    }
    if (cn->velocity > YSW_CSN_MAX_VELOCITY) {
        cn->velocity = YSW_CSN_MAX_VELOCITY;
    }
    cn->start = round_tick(cn->start);
    cn->duration = round_tick(cn->duration);
    ESP_LOGD(TAG, "normalize_cn start=%d, duration=%d, degree=%d, velocity=%d", cn->start, cn->duration, cn->degree, cn->velocity);
}

uint32_t ysw_cs_add_cn(ysw_cs_t *cs, ysw_cn_t *cn)
{
    ysw_cs_normalize_cn(cs, cn);
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
    // TODO: replace variable cs_duration with constant
    uint32_t cs_duration = ysw_cs_get_duration(cs);
    int cn_count = ysw_cs_get_cn_count(cs);
    note_t *notes = ysw_heap_allocate(sizeof(note_t) * (cn_count + 1)); // +1 for fill-to-measure
    note_t *note_p = notes;
    uint8_t tonic = cs->octave * 12;
    uint8_t root = ysw_degree_intervals[0][cs->mode % 7];
    // TODO: Revisit whether root should be 1-based, factor out a 0-based function
    root++;
    for (int j = 0; j < cn_count; j++) {
        ysw_cn_t *cn = ysw_cs_get_cn(cs, j);
        if (cn->start < cs_duration) {
            note_p->start = cn->start;
            // TODO: normalize notes to fixed duration
            if (cn->start + cn->duration > cs_duration) {
                note_p->duration = cs_duration - cn->start;
            } else {
                note_p->duration = cn->duration;
            }
            note_p->channel = 0;
            note_p->midi_note = ysw_cn_to_midi_note(cn, tonic, root) + cs->transposition;
            note_p->velocity = cn->velocity;
            note_p->instrument = cs->instrument;
            //ESP_LOGD(TAG, "ysw_cs_get_notes start=%u, duration=%d, midi_note=%d, velocity=%d, instrument=%d", note_p->start, note_p->duration, note_p->midi_note, note_p->velocity, note_p->instrument);
            end_time = note_p->start + note_p->duration;
            note_p++;
        }
    }
    uint32_t fill_to_measure = ysw_cs_get_duration(cs) - end_time;
    if (fill_to_measure) {
        note_p->start = end_time;
        note_p->duration = fill_to_measure;
        note_p->channel = 0;
        note_p->midi_note = 0; // TODO: 0-11 are reserved by ysw, 128 to 255 are reserved by application
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
    ESP_LOGD(tag, "duration=%d", ysw_cs_get_duration(cs));
    uint32_t cn_count = ysw_cs_get_cn_count(cs);
    ESP_LOGD(tag, "cn_count=%d", cn_count);
}

