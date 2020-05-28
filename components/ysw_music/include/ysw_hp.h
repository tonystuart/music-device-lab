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
#include "ysw_sn.h"
#include "ysw_cs.h"

#define YSW_STEP_NEW_MEASURE 0x01

#define YSW_HP_METRO (YSW_CS_METRO + 1)

typedef struct {
    ysw_degree_t degree;
    ysw_cs_t *cs;
    uint8_t flags;
    uint8_t state;
} ysw_step_t;

typedef struct {
    char *name;
    ysw_array_t *steps;
    uint8_t instrument;
    uint8_t octave;
    ysw_mode_t mode;
    int8_t transposition;
    uint8_t tempo;
} ysw_hp_t;

ysw_step_t *ysw_step_create(ysw_cs_t *cs, uint8_t degree, uint8_t flags);
void ysw_step_free(ysw_step_t *step);
void ysw_hp_set_name(ysw_hp_t *hp, const char* name);
int32_t ysw_hp_get_step_index(ysw_hp_t *hp, ysw_step_t *target_step);
uint32_t ysw_hp_get_steps_in_measures(ysw_hp_t *hp, uint8_t measures[], uint32_t size);
ysw_note_t *ysw_hp_get_notes(ysw_hp_t *hp, uint32_t *note_count);
void ysw_hp_set_percent_tempo(ysw_hp_t *hp);
void ysw_hp_set_instrument(ysw_hp_t *hp, uint8_t instrument);
int ysw_hp_add_cs(ysw_hp_t *hp, uint8_t degree, ysw_cs_t *cs, uint8_t flags);
int ysw_hp_add_step(ysw_hp_t *hp, ysw_step_t *new_step);
void ysw_hp_free(ysw_hp_t *hp);
ysw_hp_t *ysw_hp_create(char *name, uint8_t instrument, uint8_t octave, ysw_mode_t mode, int8_t transposition, uint8_t tempo);
ysw_hp_t *ysw_hp_copy(ysw_hp_t *old_hp);
void ysw_hp_dump(ysw_hp_t *progresssion, char *tag);

static inline ysw_step_t *ysw_hp_get_step(ysw_hp_t *hp, uint32_t index)
{
    return ysw_array_get(hp->steps, index);
}

static inline ysw_cs_t *ysw_hp_get_cs(ysw_hp_t *hp, uint32_t index)
{
    return ysw_hp_get_step(hp, index)->cs;
}

static inline ysw_degree_t ysw_hp_get_degree(ysw_hp_t *hp, uint32_t index)
{
    return ysw_hp_get_step(hp, index)->degree;
}

static inline uint32_t ysw_hp_get_step_count(ysw_hp_t *hp)
{
    return ysw_array_get_count(hp->steps);
}

static inline ysw_sn_t *ysw_step_get_sn(ysw_step_t *step, uint32_t index)
{
    return ysw_array_get(step->cs->sn_array, index);
}

static inline uint32_t ysw_step_get_sn_count(ysw_step_t *step)
{
    return ysw_array_get_count(step->cs->sn_array);
}

static inline void ysw_hp_sort_sn_array(ysw_hp_t *hp)
{
    // TODO: sort chord styles in this chord progression
}

static inline ysw_step_t *ysw_step_copy(ysw_step_t *old_step)
{
    return ysw_step_create(old_step->cs, old_step->degree, old_step->flags);
}

static inline void ysw_step_select(ysw_step_t *step, bool selected)
{
    if (selected) {
        step->state |= YSW_CSN_SELECTED;
    } else {
        step->state &= ~YSW_CSN_SELECTED;
    }
}

static inline bool ysw_step_is_selected(ysw_step_t *step)
{
    return step->state & YSW_CSN_SELECTED;
}

static inline void ysw_hp_insert_step(ysw_hp_t *hp, uint32_t index, ysw_step_t *step)
{
    ysw_array_insert(hp->steps, index, step);
}

static inline bool ysw_step_is_new_measure(ysw_step_t *step)
{
    return step->flags & YSW_STEP_NEW_MEASURE;
}

static inline void ysw_step_set_new_measure(ysw_step_t *step, bool new_measure)
{
    if (new_measure) {
        step->flags |= YSW_STEP_NEW_MEASURE;
    } else {
        step->flags &= ~YSW_STEP_NEW_MEASURE;
    }
}

