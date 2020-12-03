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

#include "assert.h"
#include "errno.h"
#include "stdio.h"
#include "stdlib.h"

#define TAG "ZM_MUSIC"

#define ZM_MF_PARTITION "/spiffs"
#define ZM_MF_CSV ZM_MF_PARTITION "/music.csv"
#define ZM_MF_TEMP ZM_MF_PARTITION "/music.tmp"

#define RECORD_SIZE 128
#define TOKENS_SIZE 20

// For drum beat and stroke, see https://en.wikipedia.org/wiki/Drum_beat

typedef enum {
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
    ZM_MF_PATTERN = 9, // formerly zm_passage_t
    ZM_MF_DIVISION = 10, // formerly zm_beat_t
    ZM_MF_MELODY = 11, // formerly zm_tone_t
    ZM_MF_CHORD = 12, // formerly zm_step_t
    ZM_MF_RHYTHM = 13, // formerly zm_drum_t

    ZM_MF_COMPOSITION = 14,
    ZM_MF_PART = 15,

    // deprecated:
    ZM_MF_MODEL = 45, // formerly PATTERN
    ZM_MF_STEP = 46, // formerly initialized zm_step_t, now zm_chord_t
    ZM_MF_SONG = 47,
    ZM_MF_ROLE = 48, // formerly PART

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
    program->patches = ysw_array_create(1);

    zm_yesno_t done = false;

    while (!done && get_tokens(zm_mfr)) {
        zm_mf_type_t type = atoi(zm_mfr->tokens[0]);
        if (type == ZM_MF_PATCH && zm_mfr->token_count == 3) {
            zm_patch_t *patch = ysw_heap_allocate(sizeof(zm_patch_t));
            patch->up_to = atoi(zm_mfr->tokens[1]);
            patch->sample = ysw_array_get(zm_mfr->music->samples, atoi(zm_mfr->tokens[2]));
            ysw_array_push(program->patches, patch);
        } else {
            push_back_tokens(zm_mfr);
            done = true;
        }
    }

    ysw_array_push(zm_mfr->music->programs, program);
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

static void parse_pattern(zm_mfr_t *zm_mfr)
{
    zm_pattern_x index = atoi(zm_mfr->tokens[1]);
    zm_medium_t count = ysw_array_get_count(zm_mfr->music->patterns);

    if (index != count) {
        ESP_LOGW(TAG, "parse_pattern index=%d, count=%d", index, count);
        return;
    }

    zm_pattern_t *pattern = ysw_heap_allocate(sizeof(zm_pattern_t));
    pattern->name = ysw_heap_strdup(zm_mfr->tokens[2]);
    pattern->tempo = atoi(zm_mfr->tokens[3]);
    pattern->key = atoi(zm_mfr->tokens[4]);
    pattern->time = atoi(zm_mfr->tokens[5]);
    pattern->melody_sample = ysw_array_get(zm_mfr->music->samples, atoi(zm_mfr->tokens[6]));
    pattern->chord_sample = ysw_array_get(zm_mfr->music->samples, atoi(zm_mfr->tokens[7]));
    pattern->divisions = ysw_array_create(16);

    zm_yesno_t done = false;
    zm_division_t *division = NULL;

    while (!done && get_tokens(zm_mfr)) {
        zm_mf_type_t type = atoi(zm_mfr->tokens[0]);
        if (type == ZM_MF_DIVISION && zm_mfr->token_count == 4) {
            division = ysw_heap_allocate(sizeof(zm_division_t));
            division->start = atoi(zm_mfr->tokens[1]);
            division->measure = atoi(zm_mfr->tokens[2]);
            division->flags = atoi(zm_mfr->tokens[3]);
            ysw_array_push(pattern->divisions, division);
        } else if (division && type == ZM_MF_MELODY && zm_mfr->token_count == 4) {
            division->melody.note = atoi(zm_mfr->tokens[1]);
            division->melody.duration = atoi(zm_mfr->tokens[2]);
            division->melody.tie = atoi(zm_mfr->tokens[3]);
        } else if (division && type == ZM_MF_CHORD && zm_mfr->token_count == 5) {
            division->chord.root = atoi(zm_mfr->tokens[1]);
            division->chord.quality = ysw_array_get(zm_mfr->music->qualities, atoi(zm_mfr->tokens[2]));
            division->chord.style = ysw_array_get(zm_mfr->music->styles, atoi(zm_mfr->tokens[3]));
            division->chord.duration = atoi(zm_mfr->tokens[4]);
        } else {
            push_back_tokens(zm_mfr);
            done = true;
        }
    }

    ysw_array_push(zm_mfr->music->patterns, pattern);
}

static void parse_model(zm_mfr_t *zm_mfr)
{
    zm_model_x index = atoi(zm_mfr->tokens[1]);
    zm_medium_t count = ysw_array_get_count(zm_mfr->music->models);

    if (index != count) {
        ESP_LOGW(TAG, "parse_model index=%d, count=%d", index, count);
        return;
    }

    zm_model_t *model = ysw_heap_allocate(sizeof(zm_model_t));
    model->name = ysw_heap_strdup(zm_mfr->tokens[2]);
    model->sample_index = atoi(zm_mfr->tokens[3]);
    model->chords = ysw_array_create(16);

    zm_yesno_t done = false;

    while (!done && get_tokens(zm_mfr)) {
        zm_mf_type_t type = atoi(zm_mfr->tokens[0]);
        if (type == ZM_MF_STEP && zm_mfr->token_count == 5) {
            zm_chord_t *chord = ysw_heap_allocate(sizeof(zm_chord_t));
            chord->root = atoi(zm_mfr->tokens[1]);
            chord->quality = ysw_array_get(zm_mfr->music->qualities, atoi(zm_mfr->tokens[2]));
            chord->style = ysw_array_get(zm_mfr->music->styles, atoi(zm_mfr->tokens[3]));
            chord->duration = atoi(zm_mfr->tokens[4]);
            ysw_array_push(model->chords, chord);
        } else {
            push_back_tokens(zm_mfr);
            done = true;
        }
    }

    ysw_array_push(zm_mfr->music->models, model);
}

static void parse_song(zm_mfr_t *zm_mfr)
{
    zm_song_x index = atoi(zm_mfr->tokens[1]);
    zm_medium_t count = ysw_array_get_count(zm_mfr->music->songs);

    if (index != count) {
        ESP_LOGW(TAG, "parse_song index=%d, count=%d", index, count);
        return;
    }

    zm_song_t *song = ysw_heap_allocate(sizeof(zm_song_t));
    song->name = ysw_heap_strdup(zm_mfr->tokens[2]);
    song->bpm = atoi(zm_mfr->tokens[3]);
    song->roles = ysw_array_create(16);

    zm_yesno_t done = false;

    while (!done && get_tokens(zm_mfr)) {
        zm_mf_type_t type = atoi(zm_mfr->tokens[0]);
        if (type == ZM_MF_ROLE && zm_mfr->token_count == 5) {
            zm_role_t *role = ysw_heap_allocate(sizeof(zm_role_t));
            role->model = ysw_array_get(zm_mfr->music->models, atoi(zm_mfr->tokens[1]));
            role->when.type = atoi(zm_mfr->tokens[2]);
            role->when.role_index = atoi(zm_mfr->tokens[3]);
            role->fit = atoi(zm_mfr->tokens[4]);
            ysw_array_push(song->roles, role);
        } else {
            push_back_tokens(zm_mfr);
            done = true;
        }
    }

    ysw_array_push(zm_mfr->music->songs, song);
}

static zm_music_t *create_music()
{
    zm_music_t *music = ysw_heap_allocate(sizeof(zm_music_t));
    music->samples = ysw_array_create(8);
    music->programs = ysw_array_create(8);
    music->qualities = ysw_array_create(32);
    music->styles = ysw_array_create(64);
    music->patterns = ysw_array_create(64);
    music->models = ysw_array_create(64);
    music->songs = ysw_array_create(16);
    return music;
}

void zm_pattern_free(zm_pattern_t *pattern)
{
    ysw_heap_free(pattern->name);
    zm_pattern_x count = ysw_array_get_count(pattern->divisions);
    for (zm_pattern_x i = 0; i < count; i++) {
        zm_pattern_t *pattern = ysw_array_get(pattern->divisions, i);
        ysw_array_free_all(pattern->divisions);
        ysw_array_free(pattern->divisions);
    }
    ysw_heap_free(pattern);
}

// TODO: factor type-specific delete functions out for reuse and invoke them

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

    zm_medium_t pattern_count = ysw_array_get_count(music->patterns);
    for (zm_medium_t i = 0; i < pattern_count; i++) {
        zm_pattern_t *pattern = ysw_array_get(music->patterns, i);
        zm_pattern_free(pattern);
    }
    ysw_array_free(music->patterns);

    zm_medium_t model_count = ysw_array_get_count(music->models);
    for (zm_medium_t i = 0; i < model_count; i++) {
        zm_model_t *model = ysw_array_get(music->models, i);
        ysw_heap_free(model->name);
        ysw_array_free_all(model->chords);
    }
    ysw_array_free(music->models);

    zm_medium_t song_count = ysw_array_get_count(music->songs);
    for (zm_medium_t i = 0; i < song_count; i++) {
        zm_song_t *song = ysw_array_get(music->songs, i);
        ysw_heap_free(song->name);
        ysw_array_free_all(song->roles);
    }
    ysw_array_free(music->songs);
}

zm_music_t *zm_read_from_file(FILE *file)
{
    zm_mfr_t *zm_mfr = &(zm_mfr_t){};

    zm_mfr->file = file;
    zm_mfr->music = create_music();

    while (get_tokens(zm_mfr)) {
        zm_mf_type_t type = atoi(zm_mfr->tokens[0]);
        if (type == ZM_MF_SAMPLE && zm_mfr->token_count == 7) {
            parse_sample(zm_mfr);
        } else if (type == ZM_MF_PROGRAM && zm_mfr->token_count == 3) {
            parse_program(zm_mfr);
        } else if (type == ZM_MF_QUALITY && zm_mfr->token_count > 4) {
            parse_quality(zm_mfr);
        } else if (type == ZM_MF_STYLE && zm_mfr->token_count == 3) {
            parse_style(zm_mfr);
        } else if (type == ZM_MF_PATTERN && zm_mfr->token_count == 8) {
            parse_pattern(zm_mfr);
        } else if (type == ZM_MF_MODEL && zm_mfr->token_count == 4) {
            parse_model(zm_mfr);
        } else if (type == ZM_MF_SONG && zm_mfr->token_count == 4) {
            parse_song(zm_mfr);
        } else {
            ESP_LOGW(TAG, "invalid record type=%d, token_count=%d", type, zm_mfr->token_count);
            for (zm_medium_t i = 0; i < zm_mfr->token_count; i++) {
                ESP_LOGW(TAG, "token[%d]=%s", i, zm_mfr->tokens[i]);
            }
        }
    }

    return zm_mfr->music;
}

zm_music_t *zm_read(void)
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
    music = zm_read_from_file(file);
    fclose(file);
    return music;
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

void zm_render_melody(ysw_array_t *notes, zm_melody_t *melody, zm_time_x melody_start, zm_channel_x channel, zm_sample_x sample_index, zm_tie_x tie)
{
    if (tie && ysw_array_get_count(notes)) {
        ysw_note_t *tied_previous = ysw_array_get_top(notes);
        if (tied_previous->midi_note == melody->note) {
            tied_previous->duration += melody->duration;
            return;
        }
    }
    ysw_note_t *note = ysw_heap_allocate(sizeof(ysw_note_t));
    note->channel = channel;
    note->midi_note = melody->note;
    note->start = melody_start;
    note->duration = melody->duration;
    note->velocity = 100;
    note->program = sample_index;
    ysw_array_push(notes, note);
}

void zm_render_chord(ysw_array_t *notes, zm_chord_t *chord, zm_time_x chord_start, zm_channel_x channel, zm_sample_x sample_index)
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
        note->start = chord_start + (sound->start * chord->duration) / YSW_TICKS_PER_MEASURE;
        note->duration = (sound->duration * chord->duration) / YSW_TICKS_PER_MEASURE;
        note->velocity = sound->velocity;
        note->program = sample_index;
        ysw_array_push(notes, note);
    }
}

zm_time_x zm_render_model(ysw_array_t *notes, zm_model_t *model, zm_time_x start_time, zm_small_t channel)
{
    zm_time_x chord_start = start_time;
    zm_medium_t chord_count = ysw_array_get_count(model->chords);
    for (zm_medium_t i = 0; i < chord_count; i++) {
        zm_chord_t *chord = ysw_array_get(model->chords, i);
        if (chord->root) { // root == 0 is a rest
            zm_render_chord(notes, chord, chord_start, channel, model->sample_index);
        }
        chord_start += chord->duration;
    }
    return chord_start;
}

ysw_array_t *zm_render_song(zm_song_t *song)
{
    zm_time_x max_time = 0;
    ysw_array_t *notes = ysw_array_create(512);
    ysw_array_t *role_times = ysw_array_create(8);
    zm_medium_t role_count = ysw_array_get_count(song->roles);
    for (zm_medium_t i = 0; i < role_count; i++) {
        zm_time_x begin_time = 0;
        zm_time_x end_time = 0;
        zm_role_t *role = ysw_array_get(song->roles, i);
        if (i == role->when.role_index) {
            begin_time = max_time;
            end_time = zm_render_model(notes, role->model, begin_time, i);
        } else if (role->when.type == ZM_WHEN_TYPE_WITH) {
            zm_role_time_t *role_time = ysw_array_get(role_times, role->when.role_index);
            begin_time = role_time->begin_time;
            if (role->fit == ZM_FIT_LOOP) {
                zm_time_x loop_time = begin_time;
                while (end_time < role_time->end_time) {
                    end_time = zm_render_model(notes, role->model, loop_time, i);
                    loop_time = end_time;
                } 
            } else {
                end_time = zm_render_model(notes, role->model, begin_time, i);
            }
        } else if (role->when.type == ZM_WHEN_TYPE_AFTER) {
            zm_role_time_t *role_time = ysw_array_get(role_times, role->when.role_index);
            begin_time = role_time->end_time;
            end_time = zm_render_model(notes, role->model, begin_time, i);
        }
        zm_role_time_t *role_time = ysw_heap_allocate(sizeof(zm_role_time_t));
        role_time->begin_time = begin_time;
        role_time->end_time = end_time;
        ysw_array_push(role_times, role_time);
        max_time = max(max_time, end_time);
    }
    ysw_array_free_all(role_times);
    ysw_array_sort(notes, zm_note_compare);
    ESP_LOGD(TAG, "song note_count=%d", ysw_array_get_count(notes));
    return notes;
}

ysw_array_t *zm_render_pattern(zm_music_t *music, zm_pattern_t *pattern, zm_channel_x base_channel)
{
    zm_tie_x tie = 0;
    zm_time_x division_time = 0;
    ysw_array_t *notes = ysw_array_create(512);
    zm_division_x division_count = ysw_array_get_count(pattern->divisions);
    zm_sample_x melody_sample_index = ysw_array_find(music->samples, pattern->melody_sample);
    zm_sample_x chord_sample_index = ysw_array_find(music->samples, pattern->chord_sample);
    for (zm_division_x i = 0; i < division_count; i++) {
        zm_division_t *division = ysw_array_get(pattern->divisions, i);
        if (division->melody.note) {
            zm_render_melody(notes, &division->melody, division_time, base_channel, melody_sample_index, tie);
            if (division->melody.tie) {
                tie = division->melody.tie;
            } else if (tie) {
                tie--;
            }
        }
        if (division->chord.root) {
            zm_render_chord(notes, &division->chord, division_time, base_channel + 1, chord_sample_index);
        }
        division_time += division->melody.duration;
    }
    ysw_array_sort(notes, zm_note_compare);
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
    { "2/2", 2, ZM_HALF },
    { "2/4", 2, ZM_QUARTER },
    { "3/4", 3, ZM_QUARTER },
    { "4/4", 4, ZM_QUARTER },
    { "6/8", 6, ZM_EIGHTH },
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

static const zm_duration_t durations[] = {
    ZM_SIXTEENTH,
    ZM_EIGHTH,
    ZM_QUARTER,
    ZM_HALF,
    ZM_WHOLE,
};

#define DURATION_SZ (sizeof(durations)/sizeof(durations[0]))

zm_duration_t zm_round_duration(zm_duration_t duration, uint8_t *ret_index, bool *ret_dotted)
{
    zm_duration_t rounded_duration = 0;
    uint8_t index = 0;
    bool dotted = false;
    for (uint8_t i = 0; i < DURATION_SZ && !rounded_duration; i++) {
        if (duration < durations[i] + (durations[i] / 4)) {
            index = i;
            dotted = false;
            rounded_duration =  durations[i];
        } else if ((duration < durations[i] + ((3 * durations[i]) / 4)) || i == (DURATION_SZ - 1)) {
            index = i;
            dotted = true;
            rounded_duration = durations[i] + (durations[i] / 2);
        }
    }
    if (ret_index) {
        *ret_index = index;
    }
    if (ret_dotted) {
        *ret_dotted = dotted;
    }
    return rounded_duration;
}

