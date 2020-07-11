// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_cs.h"
#include "stdbool.h"
#include "stdint.h"

// Step Flags

#define YSW_PS_NEW_MEASURE 0x01

// Step States

#define YSW_PS_SELECTED 0b00000001

typedef struct {
    ysw_degree_t degree;
    ysw_cs_t *cs;
    uint8_t flags; // persistent
    uint8_t state; // transient
} ysw_ps_t;

ysw_ps_t *ysw_ps_create(ysw_cs_t *cs, uint8_t degree, uint8_t flags);
void ysw_ps_free(ysw_ps_t *ps);

static inline ysw_sn_t *ysw_ps_get_sn(ysw_ps_t *ps, uint32_t index)
{
    return ysw_array_get(ps->cs->sn_array, index);
}

static inline uint32_t ysw_ps_get_sn_count(ysw_ps_t *ps)
{
    return ysw_array_get_count(ps->cs->sn_array);
}

static inline ysw_ps_t *ysw_ps_copy(ysw_ps_t *old_ps)
{
    return ysw_ps_create(old_ps->cs, old_ps->degree, old_ps->flags);
}

static inline void ysw_ps_select(ysw_ps_t *ps, bool selected)
{
    if (selected) {
        ps->state |= YSW_PS_SELECTED;
    } else {
        ps->state &= ~YSW_PS_SELECTED;
    }
}

static inline bool ysw_ps_is_selected(ysw_ps_t *ps)
{
    return ps->state & YSW_PS_SELECTED;
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


