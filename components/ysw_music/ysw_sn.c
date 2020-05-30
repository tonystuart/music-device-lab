// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_sn.h"
#include "ysw_cs.h"
#include "ysw_heap.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_CS"

ysw_sn_t *ysw_sn_create(int8_t degree, uint8_t velocity, uint32_t start, uint32_t duration, uint8_t flags)
{
    ysw_sn_t *sn = ysw_heap_allocate(sizeof(ysw_sn_t));
    sn->start = start;
    sn->duration = duration;
    sn->degree = degree;
    sn->velocity = velocity;
    sn->flags = flags;
    sn->state = 0;
    //ESP_LOGD(TAG, "create sn=%p, degree=%u, velocity=%u, start=%u, duration=%u, flags=%#x", sn, sn->degree, sn->velocity, sn->start, sn->duration, sn->flags);
    return sn;
}

ysw_sn_t *ysw_sn_copy(ysw_sn_t *sn)
{
    return ysw_sn_create(sn->degree, sn->velocity, sn->start, sn->duration, sn->flags);
}

void ysw_sn_free(ysw_sn_t *sn)
{
    assert(sn);
    //ESP_LOGD(TAG, "free sn=%p, degree=%u, velocity=%u, start=%u, duration=%u, flags=%#x", sn, sn->degree, sn->velocity, sn->start, sn->duration, sn->flags);
    ysw_heap_free(sn);
}

static inline uint32_t round_tick(uint32_t value)
{
    return ((value + YSW_CSN_TICK_INCREMENT - 1) / YSW_CSN_TICK_INCREMENT) * YSW_CSN_TICK_INCREMENT;
}

void ysw_sn_normalize(ysw_sn_t *sn)
{
    if (sn->duration > YSW_CS_DURATION) {
        sn->duration = YSW_CS_DURATION;
    }

    if (sn->start + sn->duration > YSW_CS_DURATION) {
        sn->start = YSW_CS_DURATION - sn->duration;
    }

    if (sn->degree < YSW_CSN_MIN_DEGREE) {
        sn->degree = YSW_CSN_MIN_DEGREE;
    } else if (sn->degree > YSW_CSN_MAX_DEGREE) {
        sn->degree = YSW_CSN_MAX_DEGREE;
    }

    if (sn->velocity > YSW_CSN_MAX_VELOCITY) {
        sn->velocity = YSW_CSN_MAX_VELOCITY;
    }

    sn->start = round_tick(sn->start);
    sn->duration = round_tick(sn->duration);
}

uint8_t ysw_sn_to_midi_note(ysw_sn_t *sn, uint8_t scale_tonic, uint8_t root_number)
{
    ysw_accidental_t accidental = ysw_sn_get_accidental(sn);
    uint8_t midi_note = ysw_degree_to_note(scale_tonic, root_number, sn->degree, accidental);
    return midi_note;
}

int ysw_sn_compare(const void *left, const void *right)
{
    const ysw_sn_t *left_sn = *(ysw_sn_t * const *)left;
    const ysw_sn_t *right_sn = *(ysw_sn_t * const *)right;
    uint32_t delta = left_sn->start - right_sn->start;
    if (!delta) {
        delta = left_sn->degree - right_sn->degree;
        if (!delta) {
            delta = ysw_sn_get_accidental(left_sn) - ysw_sn_get_accidental(right_sn);
            if (!delta) {
                delta = left_sn->duration - right_sn->duration;
                if (!delta) {
                    delta = left_sn->velocity - right_sn->velocity;
                }
            }
        }
    }
    return delta;
}

