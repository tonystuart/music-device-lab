// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_csn.h"
#include "ysw_cs.h"

#include "assert.h"
#include "ysw_heap.h"

#define TAG "YSW_CS"

ysw_cs_t *ysw_cs_create(char *name, uint8_t instrument, uint8_t octave, ysw_mode_t mode, int8_t transposition, uint8_t tempo, ysw_time_t time)
{
    ysw_cs_t *cs = ysw_heap_allocate(sizeof(ysw_cs_t));
    cs->name = ysw_heap_strdup(name);
    cs->csns = ysw_array_create(8);
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
    uint32_t csn_count = ysw_cs_get_csn_count(old_cs);
    ysw_cs_t *new_cs = ysw_heap_allocate(sizeof(ysw_cs_t));
    new_cs->name = ysw_heap_strdup(old_cs->name);
    new_cs->csns = ysw_array_create(csn_count);
    new_cs->instrument = old_cs->instrument;
    new_cs->octave = old_cs->octave;
    new_cs->mode = old_cs->mode;
    new_cs->transposition = old_cs->transposition;
    new_cs->tempo = old_cs->tempo;
    new_cs->time = old_cs->time;
    for (int i = 0; i < csn_count; i++) {
        ysw_csn_t *old_csn = ysw_cs_get_csn(old_cs, i);
        ysw_csn_t *new_csn = ysw_csn_copy(old_csn);
        ysw_cs_add_csn(new_cs, new_csn);
    }
    return new_cs;
}

void ysw_cs_free(ysw_cs_t *cs)
{
    uint32_t csn_count = ysw_cs_get_csn_count(cs);
    for (int i = 0; i < csn_count; i++) {
        ysw_csn_t *csn = ysw_cs_get_csn(cs, i);
        ysw_csn_free(csn);
    }
    ysw_array_free(cs->csns);
    ysw_heap_free(cs->name);
    ysw_heap_free(cs);
}

static inline uint32_t round_tick(uint32_t value)
{
    return ((value + YSW_CSN_TICK_INCREMENT - 1) / YSW_CSN_TICK_INCREMENT) * YSW_CSN_TICK_INCREMENT;
}

void ysw_cs_normalize_csn(ysw_cs_t *cs, ysw_csn_t *csn)
{
    uint32_t cs_duration = ysw_cs_get_duration(cs);
    if (csn->start > cs_duration - YSW_CSN_MIN_DURATION) {
        csn->start = cs_duration - YSW_CSN_MIN_DURATION;
    }
    if (csn->duration < YSW_CSN_MIN_DURATION) {
        csn->duration = YSW_CSN_MIN_DURATION;
    }
    if (csn->start + csn->duration > cs_duration) {
        csn->duration = cs_duration - csn->start;
    }
    if (csn->degree < YSW_CSN_MIN_DEGREE) {
        csn->degree = YSW_CSN_MIN_DEGREE;
    } else if (csn->degree > YSW_CSN_MAX_DEGREE) {
        csn->degree = YSW_CSN_MAX_DEGREE;
    }
    if (csn->velocity > YSW_CSN_MAX_VELOCITY) {
        csn->velocity = YSW_CSN_MAX_VELOCITY;
    }
    csn->start = round_tick(csn->start);
    csn->duration = round_tick(csn->duration);
    ESP_LOGD(TAG, "normalize_csn start=%d, duration=%d, degree=%d, velocity=%d", csn->start, csn->duration, csn->degree, csn->velocity);
}

uint32_t ysw_cs_add_csn(ysw_cs_t *cs, ysw_csn_t *csn)
{
    ysw_cs_normalize_csn(cs, csn);
    uint32_t index = ysw_array_push(cs->csns, csn);
    return index;
}

void ysw_cs_sort_csns(ysw_cs_t *cs)
{
    ysw_array_sort(cs->csns, ysw_csn_compare);
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

note_t *ysw_cs_get_notes(ysw_cs_t *cs, uint32_t *note_count)
{
    int end_time = 0;
    int csn_count = ysw_cs_get_csn_count(cs);
    *note_count = csn_count + 1; // +1 for fill to measure
    note_t *notes = ysw_heap_allocate(sizeof(note_t) * *note_count);
    note_t *note_p = notes;
    uint8_t tonic = cs->octave * 12;
    uint8_t root = ysw_degree_intervals[0][cs->mode % 7];
    // TODO: Revisit whether root should be 1-based, factor out a 0-based function
    root++;
    for (int j = 0; j < csn_count; j++) {
        ysw_csn_t *csn = ysw_cs_get_csn(cs, j);
        note_p->start = csn->start;
        note_p->duration = csn->duration;
        note_p->channel = 0;
        note_p->midi_note = ysw_csn_to_midi_note(csn, tonic, root) + cs->transposition;
        note_p->velocity = csn->velocity;
        note_p->instrument = cs->instrument;
        //ESP_LOGD(TAG, "ysw_cs_get_notes start=%u, duration=%d, midi_note=%d, velocity=%d, instrument=%d", note_p->start, note_p->duration, note_p->midi_note, note_p->velocity, note_p->instrument);
        end_time = note_p->start + note_p->duration;
        note_p++;
    }
    uint32_t fill_to_measure = ysw_cs_get_duration(cs) - end_time;
    note_p->start = end_time;
    note_p->duration = fill_to_measure;
    note_p->channel = 0;
    note_p->midi_note = 0; // TODO: 0-11 are reserved by ysw, 128 to 255 are reserved by application
    note_p->velocity = 0;
    note_p->instrument = 0;
    return notes;
}

void ysw_cs_dump(ysw_cs_t *cs, char *tag)
{
    ESP_LOGD(tag, "ysw_cs_dump cs=%p", cs);
    ESP_LOGD(tag, "name=%s", cs->name);
    ESP_LOGD(tag, "duration=%d", ysw_cs_get_duration(cs));
    uint32_t csn_count = ysw_cs_get_csn_count(cs);
    ESP_LOGD(tag, "csn_count=%d", csn_count);
}

