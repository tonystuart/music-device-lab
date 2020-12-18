// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "zm_music.h"

#include "ysw_common.h"
#include "ysw_csv.h"
#include "ysw_heap.h"

#include "esp_log.h"
#include "hash.h"

#include "assert.h"
#include "errno.h"
#include "fcntl.h"
#include "limits.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"

#include "sys/types.h"
#include "sys/stat.h"

#define TAG "ZM_MUSIC"

#define PATH_SIZE 128
#define RECORD_SIZE 128
#define TOKENS_SIZE 20
#define NAME_SIZE 32

// For drum beat and stroke, see https://en.wikipedia.org/wiki/Drum_beat

typedef enum {

    // settings
    ZM_MF_SETTINGS = 0,

    // instruments
    ZM_MF_SAMPLE = 1,
    ZM_MF_PROGRAM = 2,
    ZM_MF_PATCH = 3,

    // chords
    ZM_MF_QUALITY = 4,
    ZM_MF_STYLE = 5,
    ZM_MF_SOUND = 6,

    // rhythms
    ZM_MF_BEAT = 7,
    ZM_MF_STROKE = 8,

    // layers
    ZM_MF_SECTION = 9,
    ZM_MF_STEP = 10,
    ZM_MF_MELODY = 11,
    ZM_MF_CHORD = 12,
    ZM_MF_RHYTHM = 13,

    // assemblies
    ZM_MF_COMPOSITION = 14,
    ZM_MF_PART = 15,

} zm_mf_type_t;

typedef struct {
    FILE *file;
    char buffer[RECORD_SIZE];
    char *tokens[TOKENS_SIZE];
    zm_music_t *music;
    zm_small_t token_count;
    zm_large_t record_count;
    zm_yesno_t reuse_tokens;
} zm_mfr_t;

typedef struct {
    FILE *file;
    zm_music_t *music;
    hash_t *sample_map;
    hash_t *program_map;
    hash_t *quality_map;
    hash_t *style_map;
    hash_t *beat_map;
    hash_t *section_map;
    hash_t *composition_map;
} zm_mfw_t;

static hash_t *create_map(hashcount_t max_items)
{
    hash_t *hash = hash_create(max_items, NULL, NULL);
    if (!hash) {
        ESP_LOGE(TAG, "hash_create failed");
        abort();
    }
    return hash;
}

static void put_map(hash_t *map, void *key, uint32_t value)
{
    if (!hash_alloc_insert(map, key, YSW_INT_PTR value)) {
        ESP_LOGE(TAG, "hash_alloc_insert failed");
        abort();
    }
}

static uint32_t get_map(hash_t *map, void *key)
{
    hnode_t *node = hash_lookup(map, key);
    if (!node) {
        ESP_LOGE(TAG, "hash_lookup failed");
        abort();
    }
    return YSW_PTR_INT hnode_get(node);
}

static void free_map(hash_t *map)
{
    hash_free_nodes(map);
    hash_destroy(map);
}

static int get_tokens(zm_mfr_t *zm_mfr)
{
    if (zm_mfr->reuse_tokens) {
        zm_mfr->reuse_tokens = false;
        return true;
    }

    zm_yesno_t done = false;

    while (!done && fgets(zm_mfr->buffer, RECORD_SIZE, zm_mfr->file)) {
        zm_mfr->token_count = ysw_csv_parse(zm_mfr->buffer, zm_mfr->tokens, TOKENS_SIZE);
        zm_mfr->record_count++;
        if (zm_mfr->token_count > 0) {
            done = true;
        }
    }

    return done;
}

static void push_back_tokens(zm_mfr_t *zm_mfr)
{
    assert(!zm_mfr->reuse_tokens);
    zm_mfr->reuse_tokens = true;
}

static void parse_settings(zm_mfr_t *zm_mfr)
{
    zm_mfr->music->settings.clock = atoi(zm_mfr->tokens[1]);
}

static void emit_settings(zm_mfw_t *zm_mfw)
{
    fprintf(zm_mfw->file, "%d,%d\n",
            ZM_MF_SETTINGS,
            zm_mfw->music->settings.clock);
}

static void parse_sample(zm_mfr_t *zm_mfr)
{
    zm_sample_x index = atoi(zm_mfr->tokens[1]);
    zm_medium_t count = ysw_array_get_count(zm_mfr->music->samples);

    if (index != count) {
        ESP_LOGW(TAG, "parse_sample expected count=%d, got index=%d", count, index);
        return;
    }

    zm_sample_t *sample = ysw_heap_allocate(sizeof(zm_sample_t));
    sample->name = ysw_heap_strdup(zm_mfr->tokens[2]);
    sample->reppnt = atoi(zm_mfr->tokens[3]);
    sample->replen = atoi(zm_mfr->tokens[4]);
    sample->volume = atoi(zm_mfr->tokens[5]);
    sample->pan = atoi(zm_mfr->tokens[6]);

    ysw_array_push(zm_mfr->music->samples, sample);
}

static void emit_samples(zm_mfw_t *zm_mfw)
{
    uint32_t sample_count = ysw_array_get_count(zm_mfw->music->samples);
    for (uint32_t i = 0; i < sample_count; i++) {
        char name[NAME_SIZE];
        zm_sample_t *sample = ysw_array_get(zm_mfw->music->samples, i);
        put_map(zm_mfw->sample_map, sample, i);
        ysw_csv_escape(sample->name, name, sizeof(name));
        fprintf(zm_mfw->file, "%d,%d,%s,%d,%d,%d,%d\n",
                ZM_MF_SAMPLE,
                i,
                name,
                sample->reppnt,
                sample->replen,
                sample->volume,
                sample->pan);
    }
}

static void parse_program(zm_mfr_t *zm_mfr)
{
    zm_program_x index = atoi(zm_mfr->tokens[1]);
    zm_medium_t count = ysw_array_get_count(zm_mfr->music->programs);

    if (index != count) {
        ESP_LOGW(TAG, "parse_program index=%d, count=%d", index, count);
        return;
    }

    zm_program_t *program = ysw_heap_allocate(sizeof(zm_program_t));
    program->name = ysw_heap_strdup(zm_mfr->tokens[2]);
    program->type = atoi(zm_mfr->tokens[3]);
    program->gm = atoi(zm_mfr->tokens[4]);
    program->patches = ysw_array_create(1);

    zm_yesno_t done = false;

    while (!done && get_tokens(zm_mfr)) {
        zm_mf_type_t type = atoi(zm_mfr->tokens[0]);
        if (type == ZM_MF_PATCH && (zm_mfr->token_count == 3 || zm_mfr->token_count == 4)) {
            zm_patch_t *patch = ysw_heap_allocate(sizeof(zm_patch_t));
            patch->up_to = atoi(zm_mfr->tokens[1]);
            patch->sample = ysw_array_get(zm_mfr->music->samples, atoi(zm_mfr->tokens[2]));
            if (zm_mfr->token_count == 4) {
                patch->name = ysw_heap_strdup(zm_mfr->tokens[3]);
            }
            ysw_array_push(program->patches, patch);
        } else {
            push_back_tokens(zm_mfr);
            done = true;
        }
    }

    ysw_array_push(zm_mfr->music->programs, program);
}

static void emit_programs(zm_mfw_t *zm_mfw)
{
    uint32_t program_count = ysw_array_get_count(zm_mfw->music->programs);
    for (uint32_t i = 0; i < program_count; i++) {
        char name[NAME_SIZE];
        zm_program_t *program = ysw_array_get(zm_mfw->music->programs, i);
        put_map(zm_mfw->program_map, program, i);
        ysw_csv_escape(program->name, name, sizeof(name));
        fprintf(zm_mfw->file, "%d,%d,%s,%d,%d\n",
                ZM_MF_PROGRAM,
                i,
                name,
                program->type,
                program->gm);

        uint32_t patch_count = ysw_array_get_count(program->patches);
        for (uint32_t j = 0; j < patch_count; j++) {
            zm_patch_t *patch = ysw_array_get(program->patches, j);
            zm_patch_x patch_index = get_map(zm_mfw->sample_map, patch->sample);
            if (patch->name) {
                ysw_csv_escape(patch->name, name, sizeof(name));
                fprintf(zm_mfw->file, "%d,%d,%d,%s\n",
                        ZM_MF_PATCH,
                        patch->up_to,
                        patch_index,
                        name);
            } else {
                fprintf(zm_mfw->file, "%d,%d,%d\n",
                        ZM_MF_PATCH,
                        patch->up_to,
                        patch_index);
            }
        }
    }
}

#define MAX_DISTANCES 16

static void parse_quality(zm_mfr_t *zm_mfr)
{
    zm_quality_x index = atoi(zm_mfr->tokens[1]);
    zm_medium_t count = ysw_array_get_count(zm_mfr->music->qualities);

    if (index != count) {
        ESP_LOGW(TAG, "parse_quality expected count=%d, got index=%d", count, index);
        return;
    }

    zm_quality_t *quality = ysw_heap_allocate(sizeof(zm_quality_t));
    quality->name = ysw_heap_strdup(zm_mfr->tokens[2]);
    quality->label = ysw_heap_strdup(zm_mfr->tokens[3]);
    quality->distances = ysw_array_create(8);

    zm_distance_x distances_specified = zm_mfr->token_count - 4;
    zm_distance_x distances_allowed = min(distances_specified, MAX_DISTANCES);

    for (zm_small_t i = 0; i < distances_allowed; i++) {
        zm_distance_t distance = atoi(zm_mfr->tokens[4 + i]);
        ysw_array_push(quality->distances, (void*)(intptr_t)distance);
    }

    ysw_array_push(zm_mfr->music->qualities, quality);
}

static void emit_qualities(zm_mfw_t *zm_mfw)
{
    uint32_t quality_count = ysw_array_get_count(zm_mfw->music->qualities);
    for (uint32_t i = 0; i < quality_count; i++) {
        char name[NAME_SIZE];
        char label[NAME_SIZE];
        zm_quality_t *quality = ysw_array_get(zm_mfw->music->qualities, i);
        put_map(zm_mfw->quality_map, quality, i);
        ysw_csv_escape(quality->name, name, sizeof(name));
        ysw_csv_escape(quality->label, label, sizeof(label));
        fprintf(zm_mfw->file, "%d,%d,%s,%s",
                ZM_MF_QUALITY,
                i,
                name,
                label);
        zm_distance_x distance_count = ysw_array_get_count(quality->distances);
        for (uint32_t j = 0; j < distance_count; j++) {
            zm_distance_t distance = YSW_PTR_INT ysw_array_get(quality->distances, j);
            fprintf(zm_mfw->file, ",%d", distance);
        }
        fprintf(zm_mfw->file, "\n");
    }
}

static void parse_style(zm_mfr_t *zm_mfr)
{
    zm_style_x index = atoi(zm_mfr->tokens[1]);
    zm_medium_t count = ysw_array_get_count(zm_mfr->music->styles);

    if (index != count) {
        ESP_LOGW(TAG, "parse_style expected count=%d, got index=%d", count, index);
        return;
    }

    zm_style_t *style = ysw_heap_allocate(sizeof(zm_style_t));
    style->name = ysw_heap_strdup(zm_mfr->tokens[2]);
    style->sounds = ysw_array_create(8);

    zm_yesno_t done = false;
    zm_distance_x distances[MAX_DISTANCES] = {};

    while (!done && get_tokens(zm_mfr)) {
        zm_mf_type_t type = atoi(zm_mfr->tokens[0]);
        if (type == ZM_MF_SOUND && zm_mfr->token_count == 5) {
            zm_sound_t *sound = ysw_heap_allocate(sizeof(zm_sound_t));
            sound->distance_index = atoi(zm_mfr->tokens[1]);
            if (sound->distance_index < MAX_DISTANCES) {
                distances[sound->distance_index]++;
                sound->velocity = atoi(zm_mfr->tokens[2]);
                sound->start = atoi(zm_mfr->tokens[3]);
                sound->duration = atoi(zm_mfr->tokens[4]);
                ysw_array_push(style->sounds, sound);
            }
        } else {
            push_back_tokens(zm_mfr);
            done = true;
        }
    }

    for (zm_distance_x i = 0; i < MAX_DISTANCES; i++) {
        if (distances[i]) {
            style->distance_count++;
        }
    }

    ysw_array_push(zm_mfr->music->styles, style);
}

static void emit_styles(zm_mfw_t *zm_mfw)
{
    uint32_t style_count = ysw_array_get_count(zm_mfw->music->styles);
    for (uint32_t i = 0; i < style_count; i++) {
        char name[NAME_SIZE];
        zm_style_t *style = ysw_array_get(zm_mfw->music->styles, i);
        put_map(zm_mfw->style_map, style, i);
        ysw_csv_escape(style->name, name, sizeof(name));
        fprintf(zm_mfw->file, "%d,%d,%s\n",
                ZM_MF_STYLE,
                i,
                name);

        uint32_t sound_count = ysw_array_get_count(style->sounds);
        for (uint32_t j = 0; j < sound_count; j++) {
            zm_sound_t *sound = ysw_array_get(style->sounds, j);
            fprintf(zm_mfw->file, "%d,%d,%d,%d,%d\n",
                    ZM_MF_SOUND,
                    sound->distance_index,
                    sound->velocity,
                    sound->start,
                    sound->duration);
        }
    }
}

static void parse_beat(zm_mfr_t *zm_mfr)
{
    zm_beat_x index = atoi(zm_mfr->tokens[1]);
    zm_beat_x count = ysw_array_get_count(zm_mfr->music->beats);

    if (index != count) {
        ESP_LOGW(TAG, "parse_beat expected count=%d, got index=%d", count, index);
        return;
    }

    zm_beat_t *beat = ysw_heap_allocate(sizeof(zm_beat_t));
    beat->name = ysw_heap_strdup(zm_mfr->tokens[2]);
    beat->label = ysw_heap_strdup(zm_mfr->tokens[3]);
    beat->strokes = ysw_array_create(32);

    zm_yesno_t done = false;

    while (!done && get_tokens(zm_mfr)) {
        zm_mf_type_t type = atoi(zm_mfr->tokens[0]);
        if (type == ZM_MF_STROKE && zm_mfr->token_count == 4) {
            zm_stroke_t *stroke = ysw_heap_allocate(sizeof(zm_stroke_t));
            stroke->start = atoi(zm_mfr->tokens[1]);
            stroke->surface = atoi(zm_mfr->tokens[2]);
            stroke->velocity = atoi(zm_mfr->tokens[3]);
            ysw_array_push(beat->strokes, stroke);
        } else {
            push_back_tokens(zm_mfr);
            done = true;
        }
    }

    ysw_array_push(zm_mfr->music->beats, beat);
}

static void emit_beats(zm_mfw_t *zm_mfw)
{
    uint32_t beat_count = ysw_array_get_count(zm_mfw->music->beats);
    for (uint32_t i = 0; i < beat_count; i++) {
        char name[NAME_SIZE];
        char label[NAME_SIZE];
        zm_beat_t *beat = ysw_array_get(zm_mfw->music->beats, i);
        put_map(zm_mfw->beat_map, beat, i);
        ysw_csv_escape(beat->name, name, sizeof(name));
        ysw_csv_escape(beat->label, label, sizeof(label));
        fprintf(zm_mfw->file, "%d,%d,%s,%s\n",
                ZM_MF_BEAT,
                i,
                name,
                label);

        uint32_t stroke_count = ysw_array_get_count(beat->strokes);
        for (uint32_t j = 0; j < stroke_count; j++) {
            zm_stroke_t *stroke = ysw_array_get(beat->strokes, j);
            fprintf(zm_mfw->file, "%d,%d,%d,%d\n",
                    ZM_MF_STROKE,
                    stroke->start,
                    stroke->surface,
                    stroke->velocity);
        }
    }
}

static void parse_section(zm_mfr_t *zm_mfr)
{
    zm_section_x index = atoi(zm_mfr->tokens[1]);
    zm_medium_t count = ysw_array_get_count(zm_mfr->music->sections);

    if (index != count) {
        ESP_LOGW(TAG, "parse_section index=%d, count=%d", index, count);
        return;
    }

    zm_section_t *section = ysw_heap_allocate(sizeof(zm_section_t));
    section->name = ysw_heap_strdup(zm_mfr->tokens[2]);
    section->tempo = atoi(zm_mfr->tokens[3]);
    section->key = atoi(zm_mfr->tokens[4]);
    section->time = atoi(zm_mfr->tokens[5]);
    section->tlm = atoi(zm_mfr->tokens[6]);
    section->melody_program = ysw_array_get(zm_mfr->music->programs, atoi(zm_mfr->tokens[7]));
    section->chord_program = ysw_array_get(zm_mfr->music->programs, atoi(zm_mfr->tokens[8]));
    section->rhythm_program = ysw_array_get(zm_mfr->music->programs, atoi(zm_mfr->tokens[9]));
    section->steps = ysw_array_create(16);

    zm_yesno_t done = false;
    zm_step_t *step = NULL;

    while (!done && get_tokens(zm_mfr)) {
        zm_mf_type_t type = atoi(zm_mfr->tokens[0]);
        if (type == ZM_MF_STEP && zm_mfr->token_count == 4) {
            step = ysw_heap_allocate(sizeof(zm_step_t));
            step->start = atoi(zm_mfr->tokens[1]);
            step->measure = atoi(zm_mfr->tokens[2]);
            step->flags = atoi(zm_mfr->tokens[3]);
            ysw_array_push(section->steps, step);
        } else if (step && type == ZM_MF_MELODY && zm_mfr->token_count == 4) {
            step->melody.note = atoi(zm_mfr->tokens[1]);
            step->melody.duration = atoi(zm_mfr->tokens[2]);
            step->melody.tie = atoi(zm_mfr->tokens[3]);
        } else if (step && type == ZM_MF_CHORD && zm_mfr->token_count == 5) {
            step->chord.root = atoi(zm_mfr->tokens[1]);
            step->chord.quality = ysw_array_get(zm_mfr->music->qualities, atoi(zm_mfr->tokens[2]));
            step->chord.style = ysw_array_get(zm_mfr->music->styles, atoi(zm_mfr->tokens[3]));
            step->chord.duration = atoi(zm_mfr->tokens[4]);
        } else if (step && type == ZM_MF_RHYTHM && zm_mfr->token_count == 3) {
            step->rhythm.beat = ysw_array_get(zm_mfr->music->beats, atoi(zm_mfr->tokens[1]));
            step->rhythm.surface = atoi(zm_mfr->tokens[2]);
        } else {
            push_back_tokens(zm_mfr);
            done = true;
        }
    }

    ysw_array_push(zm_mfr->music->sections, section);
}

static void emit_sections(zm_mfw_t *zm_mfw)
{
    uint32_t section_count = ysw_array_get_count(zm_mfw->music->sections);
    for (uint32_t i = 0; i < section_count; i++) {
        char name[NAME_SIZE];
        zm_section_t *section = ysw_array_get(zm_mfw->music->sections, i);
        put_map(zm_mfw->section_map, section, i);
        ysw_csv_escape(section->name, name, sizeof(name));
        fprintf(zm_mfw->file, "%d,%d,%s,%d,%d,%d,%d,%d,%d,%d\n",
                ZM_MF_SECTION,
                i,
                name,
                section->tempo,
                section->key,
                section->time,
                section->tlm,
                get_map(zm_mfw->program_map, section->melody_program),
                get_map(zm_mfw->program_map, section->chord_program),
                get_map(zm_mfw->program_map, section->rhythm_program));

        uint32_t step_count = ysw_array_get_count(section->steps);
        for (uint32_t j = 0; j < step_count; j++) {
            zm_step_t *step = ysw_array_get(section->steps, j);
            fprintf(zm_mfw->file, "%d,%d,%d,%d\n",
                    ZM_MF_STEP,
                    step->start,
                    step->measure,
                    step->flags);
            if (step->melody.duration) {
                fprintf(zm_mfw->file, "%d,%d,%d,%d\n",
                        ZM_MF_MELODY,
                        step->melody.note,
                        step->melody.duration,
                        step->melody.tie);
            }
            if (step->chord.root) {
                fprintf(zm_mfw->file, "%d,%d,%d,%d,%d\n",
                        ZM_MF_CHORD,
                        step->chord.root,
                        get_map(zm_mfw->quality_map, step->chord.quality),
                        get_map(zm_mfw->style_map, step->chord.style),
                        step->chord.duration);
            }
            if (step->rhythm.beat) {
                fprintf(zm_mfw->file, "%d,%d,%d\n",
                        ZM_MF_RHYTHM,
                        get_map(zm_mfw->beat_map, step->rhythm.beat),
                        step->rhythm.surface);
            }
        }
    }
}

static void parse_composition(zm_mfr_t *zm_mfr)
{
    zm_composition_x index = atoi(zm_mfr->tokens[1]);
    zm_medium_t count = ysw_array_get_count(zm_mfr->music->compositions);

    if (index != count) {
        ESP_LOGW(TAG, "parse_composition index=%d, count=%d", index, count);
        return;
    }

    zm_composition_t *composition = ysw_heap_allocate(sizeof(zm_composition_t));
    composition->name = ysw_heap_strdup(zm_mfr->tokens[2]);
    composition->bpm = atoi(zm_mfr->tokens[3]);
    composition->parts = ysw_array_create(16);

    zm_yesno_t done = false;

    while (!done && get_tokens(zm_mfr)) {
        zm_mf_type_t type = atoi(zm_mfr->tokens[0]);
        if (type == ZM_MF_PART && zm_mfr->token_count == 6) {
            zm_part_t *part = ysw_heap_allocate(sizeof(zm_part_t));
            part->section = ysw_array_get(zm_mfr->music->sections, atoi(zm_mfr->tokens[1]));
            part->percent_volume = atoi(zm_mfr->tokens[2]);
            part->when.type = atoi(zm_mfr->tokens[3]);
            part->when.part_index = atoi(zm_mfr->tokens[4]);
            part->fit = atoi(zm_mfr->tokens[5]);
            ysw_array_push(composition->parts, part);
        } else {
            push_back_tokens(zm_mfr);
            done = true;
        }
    }

    ysw_array_push(zm_mfr->music->compositions, composition);
}

static void emit_compositions(zm_mfw_t *zm_mfw)
{
    uint32_t composition_count = ysw_array_get_count(zm_mfw->music->compositions);
    for (uint32_t i = 0; i < composition_count; i++) {
        char name[NAME_SIZE];
        zm_composition_t *composition = ysw_array_get(zm_mfw->music->compositions, i);
        put_map(zm_mfw->composition_map, composition, i);
        ysw_csv_escape(composition->name, name, sizeof(name));
        fprintf(zm_mfw->file, "%d,%d,%s,%d\n",
                ZM_MF_COMPOSITION,
                i,
                name,
                composition->bpm);

        uint32_t part_count = ysw_array_get_count(composition->parts);
        for (uint32_t j = 0; j < part_count; j++) {
            zm_part_t *part = ysw_array_get(composition->parts, j);
            fprintf(zm_mfw->file, "%d,%d,%d,%d,%d,%d\n",
                    ZM_MF_PART,
                    get_map(zm_mfw->section_map, part->section),
                    part->percent_volume,
                    part->when.type,
                    part->when.part_index,
                    part->fit);
        }
    }
}

static zm_music_t *create_music()
{
    zm_music_t *music = ysw_heap_allocate(sizeof(zm_music_t));
    music->samples = ysw_array_create(8);
    music->programs = ysw_array_create(8);
    music->qualities = ysw_array_create(32);
    music->styles = ysw_array_create(64);
    music->beats = ysw_array_create(8);
    music->sections = ysw_array_create(64);
    music->compositions = ysw_array_create(16);
    return music;
}

void zm_section_free(zm_music_t *music, zm_section_t *section)
{
    int32_t index = ysw_array_find(music->sections, section);
    assert(index >= 0);
    ysw_array_remove(music->sections, index);
    ysw_heap_free(section->name);
    ysw_array_free_all(section->steps);
    ysw_heap_free(section);
}

// TODO: factor type-specific delete functions out for reuse and invoke them
// TODO: make sure everything is being freed that was allocated

void zm_music_free(zm_music_t *music)
{
    zm_medium_t sample_count = ysw_array_get_count(music->samples);
    for (zm_medium_t i = 0; i < sample_count; i++) {
        zm_sample_t *sample = ysw_array_get(music->samples, i);
        ysw_heap_free(sample->name);
        ysw_heap_free(sample);
    }
    ysw_array_free(music->samples);

    zm_medium_t program_count = ysw_array_get_count(music->programs);
    for (zm_medium_t i = 0; i < program_count; i++) {
        zm_program_t *program = ysw_array_get(music->programs, i);
        ysw_heap_free(program->name);
        ysw_array_free_all(program->patches);
    }
    ysw_array_free(music->programs);

    zm_medium_t quality_count = ysw_array_get_count(music->qualities);
    for (zm_medium_t i = 0; i < quality_count; i++) {
        zm_quality_t *quality = ysw_array_get(music->qualities, i);
        ysw_heap_free(quality->name);
        ysw_array_free(quality->distances);
    }
    ysw_array_free(music->qualities);

    zm_medium_t style_count = ysw_array_get_count(music->styles);
    for (zm_medium_t i = 0; i < style_count; i++) {
        zm_style_t *style = ysw_array_get(music->styles, i);
        ysw_heap_free(style->name);
        ysw_array_free_all(style->sounds);
    }
    ysw_array_free(music->styles);

    zm_medium_t beat_count = ysw_array_get_count(music->qualities);
    for (zm_medium_t i = 0; i < beat_count; i++) {
        zm_beat_t *beat = ysw_array_get(music->qualities, i);
        ysw_heap_free(beat->name);
        ysw_array_free_all(beat->strokes);
    }
    ysw_array_free(music->beats);

    zm_medium_t section_count = ysw_array_get_count(music->sections);
    for (zm_medium_t i = section_count - 1; i >= 0; i--) {
        zm_section_t *section = ysw_array_get(music->sections, i);
        zm_section_free(music, section);
    }
    ysw_array_free(music->sections);

    zm_medium_t composition_count = ysw_array_get_count(music->compositions);
    for (zm_medium_t i = 0; i < composition_count; i++) {
        zm_composition_t *composition = ysw_array_get(music->compositions, i);
        ysw_heap_free(composition->name);
        ysw_array_free_all(composition->parts);
    }
    ysw_array_free(music->compositions);
}

zm_music_t *zm_parse_file(FILE *file)
{
    zm_mfr_t *zm_mfr = &(zm_mfr_t) {
        .file = file,
        .music = create_music(),
    };

    while (get_tokens(zm_mfr)) {
        zm_mf_type_t type = atoi(zm_mfr->tokens[0]);
        if (type == ZM_MF_SETTINGS && zm_mfr->token_count == 2) {
            parse_settings(zm_mfr);
        } else if (type == ZM_MF_SAMPLE && zm_mfr->token_count == 7) {
            parse_sample(zm_mfr);
        } else if (type == ZM_MF_PROGRAM && zm_mfr->token_count == 5) {
            parse_program(zm_mfr);
        } else if (type == ZM_MF_QUALITY && zm_mfr->token_count > 4) {
            parse_quality(zm_mfr);
        } else if (type == ZM_MF_STYLE && zm_mfr->token_count == 3) {
            parse_style(zm_mfr);
        } else if (type == ZM_MF_BEAT && zm_mfr->token_count == 4) {
            parse_beat(zm_mfr);
        } else if (type == ZM_MF_SECTION && zm_mfr->token_count == 10) {
            parse_section(zm_mfr);
        } else if (type == ZM_MF_COMPOSITION && zm_mfr->token_count == 4) {
            parse_composition(zm_mfr);
        } else {
            ESP_LOGW(TAG, "invalid record_count=%d, record type=%d, token_count=%d",
                    zm_mfr->record_count, type, zm_mfr->token_count);
            for (zm_medium_t i = 0; i < zm_mfr->token_count; i++) {
                ESP_LOGW(TAG, "token[%d]=%s", i, zm_mfr->tokens[i]);
            }
        }
    }

    return zm_mfr->music;
}

void zm_emit_file(FILE *file, zm_music_t *music)
{
    extern void hash_ensure_assert_off();
    hash_ensure_assert_off();

    zm_mfw_t *zm_mfw = &(zm_mfw_t){
        .file = file,
        .music = music,
        .sample_map = create_map(100),
        .program_map = create_map(100),
        .quality_map = create_map(100),
        .style_map = create_map(100),
        .beat_map = create_map(100),
        .section_map = create_map(100),
        .composition_map = create_map(100),
    };

    emit_settings(zm_mfw);
    emit_samples(zm_mfw);
    emit_programs(zm_mfw);
    emit_qualities(zm_mfw);
    emit_styles(zm_mfw);
    emit_beats(zm_mfw);
    emit_sections(zm_mfw);
    emit_compositions(zm_mfw);

    free_map(zm_mfw->sample_map);
    free_map(zm_mfw->program_map);
    free_map(zm_mfw->quality_map);
    free_map(zm_mfw->style_map);
    free_map(zm_mfw->beat_map);
    free_map(zm_mfw->section_map);
    free_map(zm_mfw->composition_map);
}

zm_music_t *zm_load_music(void)
{
    zm_music_t *music = NULL;
    FILE *file = fopen(ZM_MF_CSV, "r");
    if (!file) {
        // spiffs doesn't provide atomic rename, so check for temp file
        ESP_LOGE(TAG, "fopen file=%s failed, errno=%d", ZM_MF_CSV, errno);
        int rc = rename(ZM_MF_TEMP, ZM_MF_CSV);
        if (rc == -1) {
            ESP_LOGE(TAG, "rename old=%s new=%s failed, errno=%d", ZM_MF_TEMP, ZM_MF_CSV, errno);
            abort();
        }
        file = fopen(ZM_MF_CSV, "r");
        if (!file) {
            ESP_LOGE(TAG, "fopen file=%s failed (after rename), errno=%d", ZM_MF_CSV, errno);
            abort();
        }
    }
    music = zm_parse_file(file);
    fclose(file);
    return music;
}

void zm_save_music(zm_music_t *music)
{
    FILE *file = fopen(ZM_MF_TEMP, "w");
    if (!file) {
        ESP_LOGE(TAG, "fopen file=%s failed, errno=%d", ZM_MF_TEMP, errno);
        abort();
    }

    zm_emit_file(file, music);
    fclose(file);

    // spiffs doesn't provide atomic rename, zm_mfr_read handles recovery of partial rename
    int rc = unlink(ZM_MF_CSV);
    if (rc == -1) {
        ESP_LOGE(TAG, "unlink file=%s failed, errno=%d", ZM_MF_CSV, errno);
        abort();
    }

    rc = rename(ZM_MF_TEMP, ZM_MF_CSV);
    if (rc == -1) {
        ESP_LOGE(TAG, "rename old=%s, new=%s failed, errno=%d", ZM_MF_TEMP, ZM_MF_CSV, errno);
        abort();
    }
}

void *zm_load_sample(const char* name, uint16_t *word_count)
{
    char path[PATH_SIZE];
    snprintf(path, sizeof(path), "%s/samples/%s", ZM_MF_PARTITION, name);

    struct stat sb;
    int rc = stat(path, &sb);
    if (rc == -1) {
        ESP_LOGE(TAG, "stat failed, file=%s", path);
        abort();
    }

    int sample_size = sb.st_size;

    void *sample_data = ysw_heap_allocate(sample_size);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        ESP_LOGE(TAG, "open failed, file=%s", path);
        abort();
    }

    rc = read(fd, sample_data, sample_size);
    if (rc != sample_size) {
        ESP_LOGE(TAG, "read failed, rc=%d, sample_size=%d", rc, sample_size);
        abort();
    }

    rc = close(fd);
    if (rc == -1) {
        ESP_LOGE(TAG, "close failed");
        abort();
    }

    ESP_LOGD(TAG, "zm_load_sample: path=%s, sample_size=%d (%d words)", path, sample_size, sample_size / 2);

    if (word_count) {
        *word_count = sample_size / 2; // length is in 2 byte (16 bit) words
    }

    return sample_data;
}

#include "ysw_midi.h"
#include "ysw_note.h"
#include "ysw_ticks.h"

int zm_note_compare(const void *left, const void *right)
{
    const ysw_note_t *left_note = *(ysw_note_t * const *)left;
    const ysw_note_t *right_note = *(ysw_note_t * const *)right;
    int delta = left_note->start - right_note->start;
    if (!delta) {
        delta = left_note->channel - right_note->channel;
        if (!delta) {
            delta = left_note->midi_note - right_note->midi_note;
            if (!delta) {
                delta = left_note->duration - right_note->duration;
                if (!delta) {
                    delta = left_note->velocity - right_note->velocity;
                    if (!delta) {
                        delta = left_note->program - right_note->program;
                    }
                }
            }
        }
    }
    return delta;
}

void zm_render_note(ysw_array_t *notes, zm_channel_x channel, zm_note_t midi_note,
        zm_time_x start, zm_duration_t duration, zm_program_x program_index)
{
    ysw_note_t *note = ysw_heap_allocate(sizeof(ysw_note_t));
    note->channel = channel;
    note->midi_note = midi_note;
    note->start = start;
    note->duration = duration;
    note->velocity = 100;
    note->program = program_index;
    ysw_array_push(notes, note);
}

void zm_render_melody(ysw_array_t *notes, zm_melody_t *melody, zm_time_x melody_start,
        zm_channel_x channel, zm_program_x program_index, zm_tie_x tie)
{
    if (tie && ysw_array_get_count(notes)) {
        ysw_note_t *tied_previous = ysw_array_get_top(notes);
        if (tied_previous->midi_note == melody->note) {
            tied_previous->duration += melody->duration;
            return;
        }
    }
    zm_render_note(notes, channel, melody->note, melody_start, melody->duration, program_index);
}

#define STYLE_DURATION 1024

// NB: The start and duration of sounds in a style are based on a 1024 tick time scale,
// which is not related to ticks_per_measure.

// The chord duration is the number of ticks to fit those 1024 style ticks into.
// For example, the chord duration would be 768 for a full measure chord in 3/4 time.

void zm_render_chord(ysw_array_t *notes, zm_chord_t *chord, zm_time_x chord_start,
        zm_channel_x channel, zm_program_x program_index)
{
    zm_medium_t distance_count = ysw_array_get_count(chord->quality->distances);
    zm_medium_t sound_count = ysw_array_get_count(chord->style->sounds);
    for (zm_medium_t j = 0; j < sound_count; j++) {
        zm_sound_t *sound = ysw_array_get(chord->style->sounds, j);
        if (sound->distance_index >= distance_count) {
            ESP_LOGW(TAG, "distance_index=%d, distance_count=%d", sound->distance_index, distance_count);
            continue;
        } 
        zm_distance_t distance = (intptr_t)ysw_array_get(chord->quality->distances, sound->distance_index);
        ysw_note_t *note = ysw_heap_allocate(sizeof(ysw_note_t));
        note->channel = channel;
        note->midi_note = chord->root + distance;
        note->start = chord_start + (sound->start * chord->duration) / STYLE_DURATION;
        note->duration = (sound->duration * chord->duration) / STYLE_DURATION;
        note->velocity = sound->velocity;
        note->program = program_index;
        ysw_array_push(notes, note);
    }
}

void zm_render_beat(ysw_array_t *notes, zm_beat_t *beat, zm_time_x beat_start,
        zm_channel_x channel, zm_program_x program_index)
{
    zm_stroke_x stroke_count = ysw_array_get_count(beat->strokes);
    for (zm_stroke_x i = 0; i < stroke_count; i++) {
        zm_stroke_t *stroke = ysw_array_get(beat->strokes, i);
        ysw_note_t *note = ysw_heap_allocate(sizeof(ysw_note_t));
        note->channel = channel;
        note->midi_note = stroke->surface;
        note->start = beat_start + stroke->start;
        note->duration = 128;
        note->velocity = stroke->velocity;
        note->program = program_index;
        ysw_array_push(notes, note);
    }
}

void zm_render_rhythm(ysw_array_t *notes, zm_rhythm_t *rhythm, zm_time_x rhythm_start,
        zm_channel_x channel, zm_program_x program_index)
{
    if (rhythm->beat) {
        zm_render_beat(notes, rhythm->beat, rhythm_start, channel, program_index);
    }
    if (rhythm->surface) {
        zm_render_note(notes, channel, rhythm->surface, rhythm_start, 128, program_index);
    }
}

ysw_array_t *zm_render_step(zm_music_t *m, zm_section_t *p, zm_step_t *d, zm_channel_x bc)
{
    ysw_array_t *notes = ysw_array_create(16);
    if (d->melody.note) {
        zm_program_x mx = ysw_array_find(m->programs, p->melody_program);
        zm_render_melody(notes, &d->melody, 0, bc, mx, 0);
    }
    if (d->chord.root) {
        zm_program_x cx = ysw_array_find(m->programs, p->chord_program);
        zm_render_chord(notes, &d->chord, 0, bc + 1, cx);
    }
    if (d->rhythm.beat || d->rhythm.surface) {
        zm_program_x bx = ysw_array_find(m->programs, p->rhythm_program);
        zm_render_rhythm(notes, &d->rhythm, 0, bc + 2, bx);
    }
    ysw_array_sort(notes, zm_note_compare);
    return notes;
}

zm_time_x zm_render_section_notes(ysw_array_t *notes, zm_music_t *music, zm_section_t *section,
        zm_time_x start_time, zm_channel_x base_channel)
{
    zm_tie_x tie = 0;
    zm_step_x step_count = ysw_array_get_count(section->steps);
    zm_program_x melody_program = ysw_array_find(music->programs, section->melody_program);
    zm_program_x chord_program = ysw_array_find(music->programs, section->chord_program);
    zm_program_x rhythm_program = ysw_array_find(music->programs, section->rhythm_program);
    // two passes: one to do melody note ties, the other for chords and rhythms
    for (zm_step_x i = 0; i < step_count; i++) {
        zm_step_t *step = ysw_array_get(section->steps, i);
        if (step->melody.note) {
            zm_time_x step_start = start_time + step->start;
            zm_render_melody(notes, &step->melody, step_start, base_channel, melody_program, tie);
            if (step->melody.tie) {
                tie = step->melody.tie;
            } else if (tie) {
                tie--;
            }
        }
    }
    zm_time_x step_end = 0;
    for (zm_step_x i = 0; i < step_count; i++) {
        zm_step_t *step = ysw_array_get(section->steps, i);
        zm_time_x step_start = start_time + step->start;
        if (step->chord.root) {
            zm_render_chord(notes, &step->chord, step_start, base_channel + 1, chord_program);
        }
        if (step->rhythm.beat || step->rhythm.surface) {
            zm_render_rhythm(notes, &step->rhythm, step_start, base_channel + 2, rhythm_program);
        }
        step_end = step_start + step->melody.duration;
    }
    return step_end;
}

ysw_array_t *zm_render_section(zm_music_t *music, zm_section_t *section, zm_channel_x base_channel)
{
    ysw_array_t *notes = ysw_array_create(512);
    zm_render_section_notes(notes, music, section, 0, base_channel);
    ysw_array_sort(notes, zm_note_compare);
    return notes;
}

static void adjust_volume(ysw_array_t *notes, zm_note_x first_note, zm_percent_x percent_volume)
{
    zm_note_x note_count = ysw_array_get_count(notes);
    for (zm_note_x i = first_note; i < note_count; i++) {
        ysw_note_t *note = ysw_array_get(notes, i);
        note->velocity = (percent_volume * note->velocity) / 100;
    }
}

ysw_array_t *zm_render_composition(zm_music_t *music, zm_composition_t *composition,
        zm_channel_x base_channel)
{
    zm_time_x max_time = 0;
    ysw_array_t *notes = ysw_array_create(512);
    ysw_array_t *part_times = ysw_array_create(8);
    zm_medium_t part_count = ysw_array_get_count(composition->parts);
    for (zm_medium_t i = 0; i < part_count; i++) {
        zm_note_x first_note = ysw_array_get_count(notes);
        zm_time_x begin_time = 0;
        zm_time_x end_time = 0;
        zm_part_t *part = ysw_array_get(composition->parts, i);
        zm_channel_x channel = base_channel + (i * 3);
        if (i == part->when.part_index) {
            begin_time = max_time;
            end_time = zm_render_section_notes(notes, music, part->section, begin_time, channel);
        } else if (part->when.type == ZM_WHEN_TYPE_WITH) {
            zm_part_time_t *part_time = ysw_array_get(part_times, part->when.part_index);
            begin_time = part_time->begin_time;
            if (part->fit == ZM_FIT_LOOP) {
                zm_time_x loop_time = begin_time;
                while (end_time < part_time->end_time) {
                    end_time = zm_render_section_notes(notes, music, part->section, loop_time, channel);
                    loop_time = end_time;
                } 
            } else {
                end_time = zm_render_section_notes(notes, music, part->section, begin_time, channel);
            }
        } else if (part->when.type == ZM_WHEN_TYPE_AFTER) {
            zm_part_time_t *part_time = ysw_array_get(part_times, part->when.part_index);
            begin_time = part_time->end_time;
            end_time = zm_render_section_notes(notes, music, part->section, begin_time, channel);
        }
        if (part->percent_volume != 100) {
            adjust_volume(notes, first_note, part->percent_volume);
        }
        zm_part_time_t *part_time = ysw_heap_allocate(sizeof(zm_part_time_t));
        part_time->begin_time = begin_time;
        part_time->end_time = end_time;
        ysw_array_push(part_times, part_time);
        max_time = max(max_time, end_time);
    }
    ysw_array_free_all(part_times);
    ysw_array_sort(notes, zm_note_compare);
    ESP_LOGD(TAG, "composition note_count=%d", ysw_array_get_count(notes));
    return notes;
}

// https://en.wikipedia.org/wiki/Key_signature

static const zm_key_signature_t zm_key_signatures[] = {
    { "C",  0, 0, { 0, 0, 0, 0, 0, 0, 0 }, { 0 }, "C" },
    //              C  D  E  F  G  A  B
    { "G",  1, 0, { 0, 0, 0, 1, 0, 0, 0 }, { 0 }, "G (1#)" },
    { "D",  2, 0, { 1, 0, 0, 1, 0, 0, 0 }, { 0 }, "D (2#)" },
    { "A",  3, 0, { 1, 0, 0, 1, 1, 0, 0 }, { 0 }, "A (3#)" },
    { "E",  4, 0, { 1, 1, 0, 1, 1, 0, 0 }, { 0 }, "E (4#)" },
    { "B",  5, 0, { 1, 1, 0, 1, 1, 1, 0 }, { 0 }, "B (5#)" },
    { "F#", 6, 0, { 1, 1, 1, 1, 1, 1, 0 }, { 0 }, "F# (6#)" },
    //                     C  D  E  F  G  A  B
    { "F",  0, 1, { 0 }, { 0, 0, 0, 0, 0, 0, 1 }, "F (1b)" },
    { "Bb", 0, 2, { 0 }, { 0, 0, 1, 0, 0, 0, 1 }, "Bb (2b)" },
    { "Eb", 0, 3, { 0 }, { 0, 0, 1, 0, 0, 1, 1 }, "Eb (3b)" },
    { "Ab", 0, 4, { 0 }, { 0, 1, 1, 0, 0, 1, 1 }, "Ab (4b)" },
    { "Db", 0, 5, { 0 }, { 0, 1, 1, 0, 1, 1, 1 }, "Db (5b)" },
    { "Gb", 0, 6, { 0 }, { 1, 1, 1, 0, 1, 1, 1 }, "Gb (6b)" },
};

#define ZM_KEY_SIGNATURES (sizeof(zm_key_signatures) / sizeof(zm_key_signatures[0]))

zm_key_signature_x zm_get_next_key_index(zm_key_signature_x key_index)
{
    return (key_index + 1) % ZM_KEY_SIGNATURES;
}

const zm_key_signature_t *zm_get_key_signature(zm_key_signature_x key_index)
{
    return &zm_key_signatures[key_index % ZM_KEY_SIGNATURES];
}

// https://en.wikipedia.org/wiki/Time_signature

static const zm_time_signature_t zm_time_signatures[] = {
    { "2/2", 2, ZM_HALF, (2 * ZM_WHOLE) / 2 },
    { "2/4", 2, ZM_QUARTER, (2 * ZM_WHOLE) / 4},
    { "3/4", 3, ZM_QUARTER, (3 * ZM_WHOLE) / 4 },
    { "4/4", 4, ZM_QUARTER, (4 * ZM_WHOLE) / 4 },
    { "6/8", 6, ZM_EIGHTH, (6 * ZM_WHOLE) / 8 },
};

#define ZM_TIME_SIGNATURES (sizeof(zm_time_signatures) / sizeof(zm_time_signatures[0]))

zm_time_signature_x zm_get_next_time_index(zm_time_signature_x time_index)
{
    return (time_index + 1) % ZM_TIME_SIGNATURES;
}

const zm_time_signature_t *zm_get_time_signature(zm_time_signature_x time_index)
{
    return &zm_time_signatures[time_index % ZM_TIME_SIGNATURES];
}

// See https://en.wikipedia.org/wiki/Tempo

static const zm_tempo_signature_t zm_tempo_signatures[] = {
    { "Grave", "30 BPM", 30 },
    { "Largo", "50 BPM", 50 },
    { "Andante", "80 BPM", 80 },
    { "Moderato", "100 BPM", 100 },
    { "Allegro", "120 BPM", 120 },
    { "Vivace", "150 BPM", 150 },
    { "Presto", "180 BPM", 180 },
};

#define ZM_TEMPO_SIGNATURES (sizeof(zm_tempo_signatures) / sizeof(zm_tempo_signatures[0]))

zm_tempo_t zm_get_next_tempo_index(zm_tempo_t tempo)
{
    return (tempo + 1) % ZM_TEMPO_SIGNATURES;
}

const zm_tempo_signature_t *zm_get_tempo_signature(zm_tempo_t tempo)
{
    return &zm_tempo_signatures[tempo % ZM_TEMPO_SIGNATURES];
}

zm_bpm_x zm_tempo_to_bpm(zm_tempo_t tempo)
{
    return zm_tempo_signatures[tempo % ZM_TEMPO_SIGNATURES].bpm;
}

typedef struct {
    uint8_t index;
    zm_duration_t duration;
    zm_duration_t midpoint;
    bool dotted;
} zm_duration_map_t;

static const zm_duration_map_t durations[] = {
    { 0, ZM_SIXTEENTH, ZM_SIXTEENTH + ((ZM_EIGHTH - ZM_SIXTEENTH) / 2), false },
    { 1, ZM_EIGHTH, ZM_EIGHTH + ((ZM_QUARTER - ZM_EIGHTH) / 2), false },
    { 2, ZM_QUARTER, ZM_QUARTER + ((ZM_HALF - ZM_QUARTER) / 4), false },
    { 2, ZM_QUARTER + ((ZM_HALF - ZM_QUARTER) / 2), ZM_QUARTER + ((3 * (ZM_HALF - ZM_QUARTER)) / 4), true },
    { 3, ZM_HALF, ZM_HALF + ((ZM_WHOLE - ZM_HALF) / 2), false },
    { 3, ZM_HALF + ((ZM_WHOLE - ZM_HALF) / 2), ZM_HALF + ((3 * (ZM_WHOLE - ZM_HALF)) / 4), true },
    { 4, ZM_WHOLE, UINT_MAX, false },
};

#define DURATION_SZ (sizeof(durations)/sizeof(durations[0]))

zm_duration_t zm_round_duration(zm_duration_t duration, uint8_t *ret_index, bool *ret_dotted)
{
    for (zm_duration_t i = 0; i < DURATION_SZ; i++) {
        if (duration < durations[i].midpoint) {
            if (ret_index) {
                *ret_index = durations[i].index;
            }
            if (ret_dotted) {
                *ret_dotted = durations[i].dotted;
            }
            return durations[i].duration;
        }
    }
    assert(false);
    return 0;
}

zm_duration_t zm_get_next_dotted_duration(zm_duration_t duration, int direction)
{
    for (zm_duration_t i = 0; i < DURATION_SZ; i++) {
        if (duration < durations[i].midpoint) {
            int j = i + direction;
            if (j < 0) {
                j = DURATION_SZ - 1;
            } else if (j >= DURATION_SZ) {
                j = 0;
            }
            return durations[j].duration;
        }
    }
    assert(false);
    return 0;
}

const char *zm_get_duration_label(zm_duration_t duration)
{
    const char *label = NULL;
    switch (duration) {
        default:
        case ZM_AS_PLAYED:
            label = "As Played";
            break;
        case ZM_SIXTEENTH:
            label = "1/16";
            break;
        case ZM_EIGHTH:
            label = "1/8";
            break;
        case ZM_QUARTER:
            label = "1/4";
            break;
        case ZM_HALF:
            label = "1/2";
            break;
        case ZM_WHOLE:
            label = "1/1";
            break;
    }
    return label;
}

zm_duration_t zm_get_next_duration(zm_duration_t duration)
{
    zm_duration_t next_duration;
    if (duration == ZM_AS_PLAYED) {
        next_duration = ZM_SIXTEENTH;
    } else if (duration <= ZM_SIXTEENTH) {
        next_duration = ZM_EIGHTH;
    } else if (duration <= ZM_EIGHTH) {
        next_duration = ZM_QUARTER;
    } else if (duration <= ZM_QUARTER) {
        next_duration = ZM_HALF;
    } else if (duration <= ZM_HALF) {
        next_duration = ZM_WHOLE;
    } else {
        next_duration = ZM_AS_PLAYED;
    }
    return next_duration;
}

zm_patch_t *zm_get_patch(ysw_array_t *patches, zm_note_t midi_note)
{
    zm_patch_t *patch = NULL;
    zm_patch_x patch_count = ysw_array_get_count(patches);
    for (zm_patch_x i = 0; i < patch_count && !patch; i++) {
        zm_patch_t *candidate = ysw_array_get(patches, i);
        if (midi_note <= candidate->up_to || i == (patch_count - 1)) {
            patch = candidate;
        }
    }
    return patch;
}

// TODO: consider whether this function belongs in this module

#include "ysw_name.h"

// TODO: find a better way to get the default programs

#define DEFAULT_MELODY_PROGRAM 0
#define DEFAULT_CHORD_PROGRAM 1
#define DEFAULT_RHYTHM_PROGRAM 7

zm_section_t *zm_create_section(zm_music_t *music)
{
    char name[ZM_NAME_SZ];
    ysw_name_create(name, sizeof(name));
    zm_section_t *section = ysw_heap_allocate(sizeof(zm_section_t));
    section->name = ysw_heap_strdup(name);
    section->tempo = ZM_TEMPO_100;
    section->key = ZM_KEY_C;
    section->time = ZM_TIME_4_4;
    section->tlm = music->settings.clock++;
    section->steps = ysw_array_create(64);
    section->melody_program = ysw_array_get(music->programs, DEFAULT_MELODY_PROGRAM);
    section->chord_program = ysw_array_get(music->programs, DEFAULT_CHORD_PROGRAM);
    section->rhythm_program = ysw_array_get(music->programs, DEFAULT_RHYTHM_PROGRAM);
    return section;
}

zm_section_t *zm_copy_section(zm_music_t *music, zm_section_t *old_section)
{
    char new_name[ZM_NAME_SZ];
    ysw_name_create_new_version(old_section->name, new_name, sizeof(new_name));
    zm_section_t *new_section = ysw_heap_allocate(sizeof(zm_section_t));
    new_section->name = ysw_heap_strdup(new_name);
    new_section->tempo = old_section->tempo;
    new_section->key = old_section->key;
    new_section->time = old_section->time;
    new_section->tlm = music->settings.clock++;
    new_section->steps = ysw_array_create(64);
    new_section->melody_program = old_section->melody_program;
    new_section->chord_program = old_section->chord_program;
    new_section->rhythm_program = old_section->rhythm_program;
    zm_step_x step_count = ysw_array_get_count(old_section->steps);
    for (zm_step_x i = 0; i < step_count; i++) {
        zm_step_t *old_step = ysw_array_get(old_section->steps, i);
        zm_step_t *new_step = ysw_heap_allocate(sizeof(zm_step_t));
        *new_step = *old_step;
        ysw_array_push(new_section->steps, new_step);
    }
    return new_section;
}

ysw_array_t *zm_get_section_references(zm_music_t *music, zm_section_t *section)
{
    ysw_array_t *references = ysw_array_create(8);
    zm_composition_x composition_count = ysw_array_get_count(music->compositions);
    for (zm_composition_x i = 0; i < composition_count; i++) {
        bool found = false;
        zm_composition_t *composition = ysw_array_get(music->compositions, i);
        zm_part_x part_count = ysw_array_get_count(composition->parts);
        for (zm_part_x j = 0; j < part_count && !found; j++) {
            zm_part_t *part = ysw_array_get(composition->parts, j);
            if (part->section == section) {
                found = true;
                ysw_array_push(references, composition);
            }
        }
    }
    return references;
}
