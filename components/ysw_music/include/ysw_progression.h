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
#include "ysw_chord.h"

typedef struct {
    ysw_degree_t degree;
    ysw_cs_t *cs;
} ysw_step_t;

typedef struct {
    char *name;
    ysw_array_t *steps;
    uint8_t tonic;
    uint8_t instrument;
} ysw_progression_t;

static inline ysw_step_t *ysw_progression_get_step(ysw_progression_t *progression, uint32_t index)
{
    return ysw_array_get(progression->steps, index);
}

static inline ysw_cs_t *ysw_progression_get_cs(ysw_progression_t *progression, uint32_t index)
{
    return ysw_progression_get_step(progression, index)->cs;
}

static inline ysw_degree_t ysw_progression_get_degree(ysw_progression_t *progression, uint32_t index)
{
    return ysw_progression_get_step(progression, index)->degree;
}

static inline uint32_t ysw_progression_get_cs_count(ysw_progression_t *progression)
{
    return ysw_array_get_count(progression->steps);
}

static inline uint32_t ysw_progression_get_csn_count(ysw_progression_t *progression, uint32_t index)
{
    return ysw_cs_get_note_count(ysw_progression_get_cs(progression, index));
}

static inline ysw_csn_t *ysw_step_get_csn(ysw_step_t *step, uint32_t index)
{
    return ysw_array_get(step->cs->csns, index);
}

static inline uint32_t ysw_step_get_csn_count(ysw_step_t *step)
{
    return ysw_array_get_count(step->cs->csns);
}

static inline uint32_t ysw_step_get_duration(ysw_step_t *step)
{
    return step->cs->duration;
}

note_t *ysw_progression_get_notes(ysw_progression_t *progression);
uint32_t ysw_progression_get_note_count(ysw_progression_t *progression);
void ysw_progression_set_percent_tempo(ysw_progression_t *progression);
void ysw_progression_set_instrument(ysw_progression_t *progression, uint8_t instrument);
void ysw_progression_set_tonic(ysw_progression_t *progression, uint8_t tonic);
int ysw_progression_add_cs(ysw_progression_t *progression, uint8_t degree, ysw_cs_t *cs);
void ysw_progression_free(ysw_progression_t *progression);
ysw_progression_t *ysw_progression_create(char *name, uint8_t tonic, uint8_t instrument);
void ysw_progression_dump(ysw_progression_t *progresssion, char *tag);
