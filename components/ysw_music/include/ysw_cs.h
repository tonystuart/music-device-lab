// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_array.h"
#include "ysw_song.h"
#include "ysw_csn.h"
#include "ysw_degree.h"

// Chord Style (chord, strum, arpeggio, etc.)

typedef struct {
    char *name;
    ysw_array_t *csns;
    uint32_t duration;
} ysw_cs_t;

static inline uint32_t ysw_cs_get_csn_count(ysw_cs_t *cs)
{
    return ysw_array_get_count(cs->csns);
}

static inline ysw_csn_t *ysw_cs_get_csn(ysw_cs_t *cs, uint32_t index)
{
    return ysw_array_get(cs->csns, index);
}

void ysw_cs_set_duration(ysw_cs_t *cs, uint32_t duration);
uint32_t ysw_cs_add_csn(ysw_cs_t *cs, ysw_csn_t *csn);
void ysw_cs_free(ysw_cs_t *cs);
ysw_cs_t *ysw_cs_create();
void ysw_cs_dump(ysw_cs_t *cs, char *tag);
