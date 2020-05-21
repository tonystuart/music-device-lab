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

#define YSW_STEP_NEW_MEASURE 0x01

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
    ysw_time_t time;
} ysw_cp_t;

ysw_step_t *ysw_step_create(ysw_cs_t *cs, uint8_t degree, uint8_t flags);
void ysw_step_free(ysw_step_t *step);
void ysw_cp_set_name(ysw_cp_t *cp, const char* name);
uint32_t ysw_cp_get_steps_in_measures(ysw_cp_t *cp, uint8_t measures[], uint32_t size);
note_t *ysw_cp_get_notes(ysw_cp_t *cp, uint32_t *note_count);
uint32_t ysw_cp_get_note_count(ysw_cp_t *cp);
void ysw_cp_set_percent_tempo(ysw_cp_t *cp);
void ysw_cp_set_instrument(ysw_cp_t *cp, uint8_t instrument);
int ysw_cp_add_cs(ysw_cp_t *cp, uint8_t degree, ysw_cs_t *cs, uint8_t flags);
int ysw_cp_add_step(ysw_cp_t *cp, ysw_step_t *new_step);
void ysw_cp_free(ysw_cp_t *cp);
ysw_cp_t *ysw_cp_create(char *name, uint8_t instrument, uint8_t octave, ysw_mode_t mode, int8_t transposition, uint8_t tempo, ysw_time_t time);
void ysw_cp_dump(ysw_cp_t *progresssion, char *tag);

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

static inline uint32_t ysw_cp_get_step_count(ysw_cp_t *cp)
{
    return ysw_array_get_count(cp->steps);
}

static inline ysw_cn_t *ysw_step_get_cn(ysw_step_t *step, uint32_t index)
{
    return ysw_array_get(step->cs->cn_array, index);
}

static inline uint32_t ysw_step_get_cn_count(ysw_step_t *step)
{
    return ysw_array_get_count(step->cs->cn_array);
}

static inline void ysw_cp_sort_cn_array(ysw_cp_t *cp)
{
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

static inline uint32_t ysw_cp_get_beats_per_measure(ysw_cp_t *cp)
{
    return ysw_time_to_beats_per_measure(cp->time);
}

static inline uint32_t ysw_cp_get_beat_unit(ysw_cp_t *cp)
{
    return ysw_time_to_beat_unit(cp->time);
}

static inline uint32_t ysw_cp_get_duration(ysw_cp_t *cp)
{
    return ysw_time_to_measure_duration(cp->time);
}

static inline void ysw_cp_insert_step(ysw_cp_t *cp, uint32_t index, ysw_step_t *step)
{
    ysw_array_insert(cp->steps, index, step);
}

