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

#define TAG "YSW_CS"

ysw_cs_t *ysw_cs_create(char *name, uint32_t duration)
{
    ysw_cs_t *cs = ysw_heap_allocate(sizeof(ysw_cs_t));
    cs->name = ysw_heap_strdup(name);
    cs->csns = ysw_array_create(8);
    cs->duration = duration;
    return cs;
}

void ysw_cs_free(ysw_cs_t *cs)
{
    assert(cs);
    ysw_array_free(cs->csns);
    ysw_heap_free(cs->name);
    ysw_heap_free(cs);
}

uint32_t ysw_cs_add_note(ysw_cs_t *cs, ysw_csn_t *csn)
{
    assert(cs);
    assert(csn);
    cs->duration = max(cs->duration, csn->start + csn->duration);
    uint32_t index = ysw_array_push(cs->csns, csn);
    return index;
}

void ysw_cs_set_duration(ysw_cs_t *cs, uint32_t duration)
{
    assert(cs);
    cs->duration = duration;
}

ysw_csn_t *ysw_csn_create(int8_t degree, uint8_t velocity, uint32_t start, uint32_t duration, uint8_t flags)
{
    ESP_LOGD(TAG, "csn_create degree=%u, velocity=%u, start=%u, duration=%u", degree, velocity, start, duration);
    ysw_csn_t *ysw_csn = ysw_heap_allocate(sizeof(ysw_csn_t));
    ysw_csn->start = start;
    ysw_csn->duration = duration;
    ysw_csn->degree = degree;
    ysw_csn->velocity = velocity;
    ysw_csn->flags = flags;
    ysw_csn->state = 0;
    return ysw_csn;
}

void ysw_csn_free(ysw_csn_t *ysw_csn)
{
    assert(ysw_csn);
    ESP_LOGD(TAG, "free csn=%p, start=%u, degree=%u, csn=%u, velocity=%u", ysw_csn, ysw_csn->start, ysw_csn->degree, ysw_csn->degree, ysw_csn->velocity);
    ysw_heap_free(ysw_csn);
}

void ysw_cs_dump(ysw_cs_t *cs, char *tag)
{
    ESP_LOGD(tag, "ysw_cs_dump cs=%p", cs);
    ESP_LOGD(tag, "name=%s", cs->name);
    ESP_LOGD(tag, "duration=%d", cs->duration);
    uint32_t note_count = ysw_cs_get_note_count(cs);
    ESP_LOGD(tag, "note_count=%d", note_count);
}

uint8_t ysw_csn_to_midi_note(uint8_t scale_tonic, uint8_t root_number, ysw_csn_t *csn)
{
    ysw_accidental_t accidental = ysw_csn_get_accidental(csn);
    uint8_t midi_note = ysw_degree_to_note(scale_tonic, root_number, csn->degree, accidental);
    return midi_note;
}
