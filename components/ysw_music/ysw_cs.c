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

void ysw_cs_dump(ysw_cs_t *cs, char *tag)
{
    ESP_LOGD(tag, "ysw_cs_dump cs=%p", cs);
    ESP_LOGD(tag, "name=%s", cs->name);
    ESP_LOGD(tag, "duration=%d", cs->duration);
    uint32_t csn_count = ysw_cs_get_csn_count(cs);
    ESP_LOGD(tag, "csn_count=%d", csn_count);
}

