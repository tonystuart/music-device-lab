// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_cn.h"
#include "ysw_cs.h"

#include "assert.h"
#include "ysw_heap.h"

#define TAG "YSW_CS"

ysw_cn_t *ysw_cn_create(int8_t degree, uint8_t velocity, uint32_t start, uint32_t duration, uint8_t flags)
{
    ysw_cn_t *cn = ysw_heap_allocate(sizeof(ysw_cn_t));
    cn->start = start;
    cn->duration = duration;
    cn->degree = degree;
    cn->velocity = velocity;
    cn->flags = flags;
    cn->state = 0;
    //ESP_LOGD(TAG, "create cn=%p, degree=%u, velocity=%u, start=%u, duration=%u, flags=%#x", cn, cn->degree, cn->velocity, cn->start, cn->duration, cn->flags);
    return cn;
}

ysw_cn_t *ysw_cn_copy(ysw_cn_t *cn)
{
    return ysw_cn_create(cn->degree, cn->velocity, cn->start, cn->duration, cn->flags);
}

void ysw_cn_free(ysw_cn_t *cn)
{
    assert(cn);
    //ESP_LOGD(TAG, "free cn=%p, degree=%u, velocity=%u, start=%u, duration=%u, flags=%#x", cn, cn->degree, cn->velocity, cn->start, cn->duration, cn->flags);
    ysw_heap_free(cn);
}

static inline uint32_t round_tick(uint32_t value)
{
    return ((value + YSW_CSN_TICK_INCREMENT - 1) / YSW_CSN_TICK_INCREMENT) * YSW_CSN_TICK_INCREMENT;
}

void ysw_cn_normalize(ysw_cn_t *cn)
{
    if (cn->duration > YSW_CS_DURATION) {
        cn->duration = YSW_CS_DURATION;
        ESP_LOGD(TAG, "create_cn setting duration=%d", cn->duration);
    }
    if (cn->start + cn->duration > YSW_CS_DURATION) {
        cn->start = YSW_CS_DURATION - cn->duration;
        ESP_LOGD(TAG, "create_cn setting start=%d", cn->start);
    }

#if 0
    if (cn->start > YSW_CS_DURATION - YSW_CSN_MIN_DURATION) {
        cn->start = YSW_CS_DURATION - YSW_CSN_MIN_DURATION;
    }
    if (cn->duration < YSW_CSN_MIN_DURATION) {
        cn->duration = YSW_CSN_MIN_DURATION;
    }
    if (cn->start + cn->duration > YSW_CS_DURATION) {
        cn->duration = YSW_CS_DURATION - cn->start;
    }
#endif

    if (cn->degree < YSW_CSN_MIN_DEGREE) {
        cn->degree = YSW_CSN_MIN_DEGREE;
    } else if (cn->degree > YSW_CSN_MAX_DEGREE) {
        cn->degree = YSW_CSN_MAX_DEGREE;
    }

    if (cn->velocity > YSW_CSN_MAX_VELOCITY) {
        cn->velocity = YSW_CSN_MAX_VELOCITY;
    }

    cn->start = round_tick(cn->start);
    cn->duration = round_tick(cn->duration);

    ESP_LOGD(TAG, "normalize_cn start=%d, duration=%d, degree=%d, velocity=%d", cn->start, cn->duration, cn->degree, cn->velocity);
}

uint8_t ysw_cn_to_midi_note(ysw_cn_t *cn, uint8_t scale_tonic, uint8_t root_number)
{
    ysw_accidental_t accidental = ysw_cn_get_accidental(cn);
    uint8_t midi_note = ysw_degree_to_note(scale_tonic, root_number, cn->degree, accidental);
    return midi_note;
}

int ysw_cn_compare(const void *left, const void *right)
{
    const ysw_cn_t *left_cn = *(ysw_cn_t * const *)left;
    const ysw_cn_t *right_cn = *(ysw_cn_t * const *)right;
    uint32_t delta = left_cn->start - right_cn->start;
    if (!delta) {
        delta = left_cn->degree - right_cn->degree;
        if (!delta) {
            delta = ysw_cn_get_accidental(left_cn) - ysw_cn_get_accidental(right_cn);
            if (!delta) {
                delta = left_cn->duration - right_cn->duration;
                if (!delta) {
                    delta = left_cn->velocity - right_cn->velocity;
                }
            }
        }
    }
    return delta;
}

