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

