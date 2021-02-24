// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_mod_music.h"
#include "ysw_heap.h"
#include "ysw_midi.h"
#include "ysw_mod_synth.h"
#include "zm_music.h"
#include "hash.h"
#include "esp_log.h"
#include "assert.h"
#include "stdlib.h"

#define TAG "YSW_MOD_MUSIC"

typedef struct {
    zm_music_t *music;
    hash_t *sample_map;
} ysw_mod_music_t;

ysw_mod_sample_t *provide_sample(void *context, uint8_t program_index, uint8_t midi_note, bool *is_new)
{
    assert(context);
    assert(program_index <= YSW_MIDI_MAX);

    ysw_mod_music_t *ysw_mod_music = context;

    if (program_index >= ysw_array_get_count(ysw_mod_music->music->programs)) {
        ESP_LOGW(TAG, "invalid program=%d, substituting program=0", program_index);
        program_index = 0;
    }

    zm_program_t *program = ysw_array_get(ysw_mod_music->music->programs, program_index);
    zm_patch_t *patch = zm_get_patch(program->patches, midi_note);
    assert(patch && patch->sample);
    zm_sample_t *sample = patch->sample;

    ysw_mod_sample_t *mod_sample = NULL;
    hnode_t *node = hash_lookup(ysw_mod_music->sample_map, sample);
    if (node) {
        mod_sample = hnode_get(node);
        if (is_new) {
            *is_new = false;
        }
    } else {
        mod_sample = ysw_heap_allocate(sizeof(ysw_mod_sample_t));
        mod_sample->data = zm_load_sample(sample->name, &mod_sample->length);
        mod_sample->reppnt = sample->reppnt;
        mod_sample->replen = sample->replen;
        mod_sample->volume = sample->volume;
        mod_sample->pan = sample->pan;
        mod_sample->fine_tune = sample->fine_tune;
        mod_sample->root_key = sample->root_key;
        mod_sample->delay = sample->delay;
        mod_sample->attack = sample->attack;
        mod_sample->hold = sample->hold;
        mod_sample->decay = sample->decay;
        mod_sample->release = sample->release;
        mod_sample->sustain = sample->sustain;

        if (!hash_alloc_insert(ysw_mod_music->sample_map, sample, mod_sample)) {
            ESP_LOGE(TAG, "hash_alloc_insert failed");
            abort();
        }

        if (is_new) {
            *is_new = true;
        }
    }

    return mod_sample;
}

ysw_mod_host_t *ysw_mod_music_create_host(zm_music_t *music)
{
    ysw_mod_music_t *ysw_mod_music = ysw_heap_allocate(sizeof(ysw_mod_music_t));
    ysw_mod_music->music = music;
    ysw_mod_music->sample_map = hash_create(50, NULL, NULL);

    ysw_mod_host_t *mod_host = ysw_heap_allocate(sizeof(ysw_mod_host_t));
    mod_host->context = ysw_mod_music;
    mod_host->provide_sample = provide_sample;

    return mod_host;
}
