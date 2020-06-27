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

#define YSW_PS_NEW_MEASURE 0x01

#define HP_NAME_SZ 64

typedef struct {
    ysw_degree_t degree;
    ysw_cs_t *cs;
    uint8_t flags;
    uint8_t state;
} ysw_ps_t;

typedef struct {
    char *name;
    ysw_array_t *ps_array;
    uint8_t instrument;
    uint8_t octave;
    ysw_mode_t mode;
    int8_t transposition;
    uint8_t tempo;
} ysw_hp_t;

ysw_ps_t *ysw_ps_create(ysw_cs_t *cs, uint8_t degree, uint8_t flags);
void ysw_ps_free(ysw_ps_t *ps);
void ysw_hp_set_name(ysw_hp_t *hp, const char* name);
int32_t ysw_hp_get_ps_index(ysw_hp_t *hp, ysw_ps_t *target_ps);
uint32_t ysw_hp_get_spm(ysw_hp_t *hp, uint8_t measures[], uint32_t size);
ysw_note_t *ysw_hp_get_notes(ysw_hp_t *hp, uint32_t *note_count);
ysw_note_t *ysw_hp_get_step_notes(ysw_hp_t *hp, ysw_ps_t *ps, uint32_t *note_count);
void ysw_hp_set_percent_tempo(ysw_hp_t *hp);
void ysw_hp_set_instrument(ysw_hp_t *hp, uint8_t instrument);
int ysw_hp_add_cs(ysw_hp_t *hp, uint8_t degree, ysw_cs_t *cs, uint8_t flags);
int ysw_hp_add_ps(ysw_hp_t *hp, ysw_ps_t *new_ps);
void ysw_hp_free(ysw_hp_t *hp);
ysw_hp_t *ysw_hp_create(char *name, uint8_t instrument, uint8_t octave, ysw_mode_t mode, int8_t transposition, uint8_t tempo);
ysw_hp_t *ysw_hp_copy(ysw_hp_t *old_hp);
void ysw_hp_dump(ysw_hp_t *progresssion, char *tag);
uint32_t ysw_hp_get_selection_count(ysw_hp_t *hp);
int32_t ysw_hp_find_previous_selected_ps(ysw_hp_t *hp, uint32_t ps_index, bool is_wrap);
int32_t ysw_hp_find_next_selected_ps(ysw_hp_t *hp, uint32_t ps_index, bool is_wrap);

static inline ysw_ps_t *ysw_hp_get_ps(ysw_hp_t *hp, uint32_t index)
{
    return ysw_array_get(hp->ps_array, index);
}

static inline ysw_cs_t *ysw_hp_get_cs(ysw_hp_t *hp, uint32_t index)
{
    return ysw_hp_get_ps(hp, index)->cs;
}

static inline ysw_degree_t ysw_hp_get_degree(ysw_hp_t *hp, uint32_t index)
{
    return ysw_hp_get_ps(hp, index)->degree;
}

static inline uint32_t ysw_hp_get_ps_count(ysw_hp_t *hp)
{
    return ysw_array_get_count(hp->ps_array);
}

static inline ysw_sn_t *ysw_ps_get_sn(ysw_ps_t *ps, uint32_t index)
{
    return ysw_array_get(ps->cs->sn_array, index);
}

static inline uint32_t ysw_ps_get_sn_count(ysw_ps_t *ps)
{
    return ysw_array_get_count(ps->cs->sn_array);
}

static inline void ysw_hp_sort_sn_array(ysw_hp_t *hp)
{
    // TODO: sort chord styles in this chord progression
}

static inline ysw_ps_t *ysw_ps_copy(ysw_ps_t *old_ps)
{
    return ysw_ps_create(old_ps->cs, old_ps->degree, old_ps->flags);
}

static inline void ysw_ps_select(ysw_ps_t *ps, bool selected)
{
    if (selected) {
        ps->state |= YSW_CSN_SELECTED;
    } else {
        ps->state &= ~YSW_CSN_SELECTED;
    }
}

static inline bool ysw_ps_is_selected(ysw_ps_t *ps)
{
    return ps->state & YSW_CSN_SELECTED;
}

static inline void ysw_hp_insert_ps(ysw_hp_t *hp, uint32_t index, ysw_ps_t *ps)
{
    ysw_array_insert(hp->ps_array, index, ps);
}

static inline bool ysw_ps_is_new_measure(ysw_ps_t *ps)
{
    return ps->flags & YSW_PS_NEW_MEASURE;
}

static inline void ysw_ps_set_new_measure(ysw_ps_t *ps, bool new_measure)
{
    if (new_measure) {
        ps->flags |= YSW_PS_NEW_MEASURE;
    } else {
        ps->flags &= ~YSW_PS_NEW_MEASURE;
    }
}

