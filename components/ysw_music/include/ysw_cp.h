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
#include "ysw_cn.h"
#include "ysw_cs.h"

typedef struct {
    ysw_degree_t degree;
    ysw_cs_t *cs;
} ysw_step_t;

typedef struct {
    char *name;
    ysw_array_t *steps;
    uint8_t tonic;
    uint8_t instrument;
} ysw_cp_t;

static inline ysw_step_t *ysw_cp_get_step(ysw_cp_t *cp, uint32_t index)
{
    return ysw_array_get(cp->steps, index);
}

static inline ysw_cs_t *ysw_cp_get_cs(ysw_cp_t *cp, uint32_t index)
{
    return ysw_cp_get_step(cp, index)->cs;
}

static inline ysw_degree_t ysw_cp_get_degree(ysw_cp_t *cp, uint32_t index)
{
    return ysw_cp_get_step(cp, index)->degree;
}

static inline uint32_t ysw_cp_get_cs_count(ysw_cp_t *cp)
{
    return ysw_array_get_count(cp->steps);
}

static inline uint32_t ysw_cp_get_cn_count(ysw_cp_t *cp, uint32_t index)
{
    return ysw_cs_get_cn_count(ysw_cp_get_cs(cp, index));
}

static inline ysw_cn_t *ysw_step_get_cn(ysw_step_t *step, uint32_t index)
{
    return ysw_array_get(step->cs->cn_array, index);
}

static inline uint32_t ysw_step_get_cn_count(ysw_step_t *step)
{
    return ysw_array_get_count(step->cs->cn_array);
}

static inline uint32_t ysw_step_get_duration(ysw_step_t *step)
{
    return ysw_cs_get_duration(step->cs);
}

note_t *ysw_cp_get_notes(ysw_cp_t *cp);
uint32_t ysw_cp_get_note_count(ysw_cp_t *cp);
void ysw_cp_set_percent_tempo(ysw_cp_t *cp);
void ysw_cp_set_instrument(ysw_cp_t *cp, uint8_t instrument);
void ysw_cp_set_tonic(ysw_cp_t *cp, uint8_t tonic);
int ysw_cp_add_cs(ysw_cp_t *cp, uint8_t degree, ysw_cs_t *cs);
void ysw_cp_free(ysw_cp_t *cp);
ysw_cp_t *ysw_cp_create(char *name, uint8_t tonic, uint8_t instrument);
void ysw_cp_dump(ysw_cp_t *progresssion, char *tag);
