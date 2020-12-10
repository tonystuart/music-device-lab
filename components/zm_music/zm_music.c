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
    pattern->melody_program = ysw_array_get(zm_mfr->music->programs, atoi(zm_mfr->tokens[6]));
    pattern->chord_program = ysw_array_get(zm_mfr->music->programs, atoi(zm_mfr->tokens[7]));
    pattern->rhythm_program = ysw_array_get(zm_mfr->music->programs, atoi(zm_mfr->tokens[8]));
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
        } else if (division && type == ZM_MF_RHYTHM && zm_mfr->token_count == 2) {
            division->rhythm.beat = ysw_array_get(zm_mfr->music->beats, atoi(zm_mfr->tokens[1]));
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
    music->beats = ysw_array_create(8);
    music->patterns = ysw_array_create(64);
    music->models = ysw_array_create(64);
    music->songs = ysw_array_create(16);
    return music;
}

void zm_pattern_free(zm_pattern_t *pattern)
{
    ysw_heap_free(pattern->name);
    ysw_array_free_all(pattern->divisions);
    ysw_heap_free(pattern);
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

zm_music_t *zm_parse_file(FILE *file)
{
    zm_mfr_t *zm_mfr = &(zm_mfr_t){};

    zm_mfr->file = file;
    zm_mfr->music = create_music();

    while (get_tokens(zm_mfr)) {
        zm_mf_type_t type = atoi(zm_mfr->tokens[0]);
        if (type == ZM_MF_SAMPLE && zm_mfr->token_count == 7) {
            parse_sample(zm_mfr);
        } else if (type == ZM_MF_PROGRAM && zm_mfr->token_count == 5) {
            parse_program(zm_mfr);
        } else if (type == ZM_MF_QUALITY && zm_mfr->token_count > 4) {
            parse_quality(zm_mfr);
        } else if (type == ZM_MF_STYLE && zm_mfr->token_count == 3) {
            parse_style(zm_mfr);
        } else if (type == ZM_MF_BEAT && zm_mfr->token_count == 4) {
            parse_beat(zm_mfr);
        } else if (type == ZM_MF_PATTERN && zm_mfr->token_count == 9) {
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

ysw_array_t *zm_render_division(zm_music_t *m, zm_pattern_t *p, zm_division_t *d, zm_channel_x bc)
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

ysw_array_t *zm_render_pattern(zm_music_t *music, zm_pattern_t *pattern, zm_channel_x base_channel)
{
    zm_tie_x tie = 0;
    ysw_array_t *notes = ysw_array_create(512);
    zm_division_x division_count = ysw_array_get_count(pattern->divisions);
    zm_program_x melody_program = ysw_array_find(music->programs, pattern->melody_program);
    zm_program_x chord_program = ysw_array_find(music->programs, pattern->chord_program);
    zm_program_x rhythm_program = ysw_array_find(music->programs, pattern->rhythm_program);
    // two passes: one to enable melody note ties, the other for chords and rhythms
    for (zm_division_x i = 0; i < division_count; i++) {
        zm_division_t *division = ysw_array_get(pattern->divisions, i);
        if (division->melody.note) {
            zm_render_melody(notes, &division->melody, division->start, base_channel, melody_program, tie);
            if (division->melody.tie) {
                tie = division->melody.tie;
            } else if (tie) {
                tie--;
            }
        }
    }
    for (zm_division_x i = 0; i < division_count; i++) {
        zm_division_t *division = ysw_array_get(pattern->divisions, i);
        if (division->chord.root) {
            zm_render_chord(notes, &division->chord, division->start, base_channel + 1, chord_program);
        }
        if (division->rhythm.beat || division->rhythm.surface) {
            zm_render_rhythm(notes, &division->rhythm, division->start, base_channel + 2, rhythm_program);
        }
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
    { 2, ZM_QUARTER, ZM_QUARTER + ((3 * (ZM_HALF - ZM_QUARTER)) / 4), true },
    { 3, ZM_HALF, ZM_HALF + ((ZM_WHOLE - ZM_HALF) / 2), false },
    { 3, ZM_HALF, ZM_HALF + ((3 * (ZM_WHOLE - ZM_HALF)) / 4), true },
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

zm_pattern_t *zm_music_create_pattern(zm_music_t *music)
{
    char name[32];
    ysw_name_create(name, sizeof(name));
    zm_pattern_t *pattern = ysw_heap_allocate(sizeof(zm_pattern_t));
    pattern->name = ysw_heap_strdup(name);
    pattern->divisions = ysw_array_create(64);
    pattern->key = ZM_KEY_C;
    pattern->time = ZM_TIME_4_4;
    pattern->tempo = ZM_TEMPO_100;
    pattern->melody_program = ysw_array_get(music->programs, DEFAULT_MELODY_PROGRAM);
    pattern->chord_program = ysw_array_get(music->programs, DEFAULT_CHORD_PROGRAM);
    pattern->rhythm_program = ysw_array_get(music->programs, DEFAULT_RHYTHM_PROGRAM);
    return pattern;
}
