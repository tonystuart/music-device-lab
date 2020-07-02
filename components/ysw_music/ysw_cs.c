// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_cs.h"
#include "ysw_heap.h"
#include "ysw_sn.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_CS"

// TODO: consider inlining one liners

ysw_cs_t* ysw_cs_create(char *name, uint8_t instrument, uint8_t octave, ysw_mode_t mode, int8_t transposition, uint8_t tempo, uint8_t divisions)
{
    ysw_cs_t *cs = ysw_heap_allocate(sizeof(ysw_cs_t));
    cs->name = ysw_heap_strdup(name);
    cs->sn_array = ysw_array_create(8);
    cs->instrument = instrument;
    cs->octave = octave;
    cs->mode = mode;
    cs->transposition = transposition;
    cs->tempo = tempo;
    cs->divisions = divisions;
    return cs;
}

ysw_cs_t* ysw_cs_copy(ysw_cs_t *old_cs)
{
    uint32_t sn_count = ysw_cs_get_sn_count(old_cs);
    ysw_cs_t *new_cs = ysw_heap_allocate(sizeof(ysw_cs_t));
    new_cs->name = ysw_heap_strdup(old_cs->name);
    new_cs->sn_array = ysw_array_create(sn_count);
    new_cs->instrument = old_cs->instrument;
    new_cs->octave = old_cs->octave;
    new_cs->mode = old_cs->mode;
    new_cs->transposition = old_cs->transposition;
    new_cs->tempo = old_cs->tempo;
    new_cs->divisions = old_cs->divisions;
    for (int i = 0; i < sn_count; i++) {
        ysw_sn_t *old_sn = ysw_cs_get_sn(old_cs, i);
        ysw_sn_t *new_sn = ysw_sn_copy(old_sn);
        ysw_cs_add_sn(new_cs, new_sn);
    }
    return new_cs;
}

void ysw_cs_free(ysw_cs_t *cs)
{
    uint32_t sn_count = ysw_cs_get_sn_count(cs);
    for (int i = 0; i < sn_count; i++) {
        ysw_sn_t *sn = ysw_cs_get_sn(cs, i);
        ysw_sn_free(sn);
    }
    ysw_array_free(cs->sn_array);
    ysw_heap_free(cs->name);
    ysw_heap_free(cs);
}

uint32_t ysw_cs_add_sn(ysw_cs_t *cs, ysw_sn_t *sn)
{
    return ysw_array_push(cs->sn_array, sn);
}

void ysw_cs_sort_sn_array(ysw_cs_t *cs)
{
    ysw_array_sort(cs->sn_array, ysw_sn_compare);
}

void ysw_cs_set_name(ysw_cs_t *cs, const char *name)
{
    cs->name = ysw_heap_string_reallocate(cs->name, name);
}

typedef struct {
    ysw_cs_t *cs;
    uint8_t tonic;
    uint8_t root;
    ysw_note_t *notes;
    ysw_note_t *note_p;
    uint32_t sn_count;
    uint32_t metronome_index;
    uint32_t metronome_tick;
    uint32_t end_time;
} mc_t;

static mc_t* initialize_context(mc_t *mc)
{
    mc->sn_count = ysw_cs_get_sn_count(mc->cs);
    uint32_t max_note_count = mc->sn_count;
    max_note_count += mc->cs->divisions; // metronome ticks
    max_note_count += 1; // fill-to-measure
    mc->notes = ysw_heap_allocate(sizeof(ysw_note_t) * max_note_count);
    mc->note_p = mc->notes;
    mc->tonic = mc->cs->octave * 12;
    mc->root = ysw_degree_intervals[0][mc->cs->mode % 7] + 1; // +1 because root is 1-based
    return mc;
}

static void add_metro_markers(mc_t *mc, uint32_t up_to_tick)
{
    while (mc->metronome_tick <= up_to_tick) {
        mc->note_p->start = mc->metronome_tick;
        mc->note_p->duration = 0;
        mc->note_p->channel = YSW_MIDI_STATUS_CHANNEL;
        mc->note_p->midi_note = YSW_MIDI_STATUS_NOTE;
        mc->note_p->velocity = 0;
        mc->note_p->instrument = 0;
        mc->note_p++;
        mc->metronome_tick = (++mc->metronome_index * YSW_CS_DURATION) / mc->cs->divisions;
    }
}

static void add_note(mc_t *mc, ysw_sn_t *sn)
{
    mc->note_p->start = sn->start;
    mc->note_p->duration = sn->duration;
    mc->note_p->channel = YSW_CS_MUSIC_CHANNEL;
    mc->note_p->midi_note = ysw_sn_to_midi_note(sn, mc->tonic, mc->root) + mc->cs->transposition;
    mc->note_p->velocity = sn->velocity;
    mc->note_p->instrument = mc->cs->instrument;
    mc->end_time = mc->note_p->start + mc->note_p->duration;
    mc->note_p++;
}

ysw_note_t* ysw_cs_get_notes(ysw_cs_t *cs, uint32_t *note_count)
{
    ysw_cs_sort_sn_array(cs);
    mc_t *mc = initialize_context(&(mc_t ) { .cs = cs });
    for (int j = 0; j < mc->sn_count; j++) {
        ysw_sn_t *sn = ysw_cs_get_sn(mc->cs, j);
        add_metro_markers(mc, sn->start);
        add_note(mc, sn);
    }
    add_metro_markers(mc, YSW_CS_DURATION);
    *note_count = mc->note_p - mc->notes;
    return mc->notes;
}

ysw_note_t* ysw_cs_get_note(ysw_cs_t *cs, ysw_sn_t *sn)
{
    ysw_note_t *notes = ysw_heap_allocate(sizeof(ysw_note_t));
    ysw_note_t *note_p = notes;
    uint8_t tonic = cs->octave * 12;
    uint8_t root = ysw_degree_intervals[0][cs->mode % 7] + 1; // +1 because root is 1-based
    note_p->start = 0;
    note_p->duration = sn->duration;
    note_p->channel = YSW_CS_MUSIC_CHANNEL;
    note_p->midi_note = ysw_sn_to_midi_note(sn, tonic, root) + cs->transposition;
    note_p->velocity = sn->velocity;
    note_p->instrument = cs->instrument;
    return notes;
}

uint32_t ysw_cs_insert_sn(ysw_cs_t *cs, ysw_sn_t *sn)
{
    // TODO: consider in-order insertion
    ysw_array_push(cs->sn_array, sn);
    ysw_cs_sort_sn_array(cs);
    return ysw_cs_get_sn_index(cs, sn);
}

