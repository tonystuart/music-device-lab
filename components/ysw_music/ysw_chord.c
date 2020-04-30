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

uint32_t ysw_cs_add_csn(ysw_cs_t *cs, ysw_csn_t *csn)
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
    ysw_csn_t *csn = ysw_heap_allocate(sizeof(ysw_csn_t));
    csn->start = start;
    csn->duration = duration;
    csn->degree = degree;
    csn->velocity = velocity;
    csn->flags = flags;
    csn->state = 0;
    ESP_LOGD(TAG, "create csn=%p, degree=%u, velocity=%u, start=%u, duration=%u, flags=%#x", csn, csn->degree, csn->velocity, csn->start, csn->duration, csn->flags);
    return csn;
}

ysw_csn_t *ysw_csn_copy(ysw_csn_t *csn)
{
    return ysw_csn_create(csn->degree, csn->velocity, csn->start, csn->duration, csn->flags);
}

void ysw_csn_free(ysw_csn_t *csn)
{
    assert(csn);
    ESP_LOGD(TAG, "free csn=%p, degree=%u, velocity=%u, start=%u, duration=%u, flags=%#x", csn, csn->degree, csn->velocity, csn->start, csn->duration, csn->flags);
    ysw_heap_free(csn);
}

void ysw_cs_dump(ysw_cs_t *cs, char *tag)
{
    ESP_LOGD(tag, "ysw_cs_dump cs=%p", cs);
    ESP_LOGD(tag, "name=%s", cs->name);
    ESP_LOGD(tag, "duration=%d", cs->duration);
    uint32_t csn_count = ysw_cs_get_csn_count(cs);
    ESP_LOGD(tag, "csn_count=%d", csn_count);
}

uint8_t ysw_csn_to_midi_note(uint8_t scale_tonic, uint8_t root_number, ysw_csn_t *csn)
{
    ysw_accidental_t accidental = ysw_csn_get_accidental(csn);
    uint8_t midi_note = ysw_degree_to_note(scale_tonic, root_number, csn->degree, accidental);
    return midi_note;
}
