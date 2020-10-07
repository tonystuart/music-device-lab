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
#define ZM_MF_CSV ZM_MF_PARTITION "/zm_music.csv"
#define ZM_MF_TEMP ZM_MF_PARTITION "/zm_music.tmp"

#define RECORD_SIZE 128
#define TOKENS_SIZE 20

typedef enum {
    ZM_MF_SAMPLE = 1,
    ZM_MF_QUALITY,
    ZM_MF_STYLE,
    ZM_MF_SOUND,
    ZM_MF_PATTERN,
    ZM_MF_STEP,
    ZM_MF_SONG,
    ZM_MF_PART,
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
    zm_index_t index = atoi(zm_mfr->tokens[1]);
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

static void parse_quality(zm_mfr_t *zm_mfr)
{
    zm_index_t index = atoi(zm_mfr->tokens[1]);
    zm_medium_t count = ysw_array_get_count(zm_mfr->music->qualities);

    if (index != count) {
        ESP_LOGW(TAG, "parse_quality expected count=%d, got index=%d", count, index);
        return;
    }

    zm_quality_t *quality = ysw_heap_allocate(sizeof(zm_quality_t));
    quality->name = ysw_heap_strdup(zm_mfr->tokens[2]);
    quality->semitones = ysw_array_create(8);
    
    for (zm_small_t i = 3; i < zm_mfr->token_count; i++) {
        zm_semitone_t semitone = atoi(zm_mfr->tokens[i]);
        ysw_array_push(quality->semitones, (void*)(uintptr_t)semitone);
    }

    ysw_array_push(zm_mfr->music->qualities, quality);
}

static void parse_style(zm_mfr_t *zm_mfr)
{
    zm_index_t index = atoi(zm_mfr->tokens[1]);
    zm_medium_t count = ysw_array_get_count(zm_mfr->music->styles);

    if (index != count) {
        ESP_LOGW(TAG, "parse_style expected count=%d, got index=%d", count, index);
        return;
    }

    zm_style_t *style = ysw_heap_allocate(sizeof(zm_style_t));
    style->name = ysw_heap_strdup(zm_mfr->tokens[2]);
    style->sounds = ysw_array_create(8);

    zm_yesno_t done = false;

    while (!done && get_tokens(zm_mfr)) {
        zm_mf_type_t type = atoi(zm_mfr->tokens[0]);
        if (type == ZM_MF_SOUND && zm_mfr->token_count == 5) {
            zm_sound_t *sound = ysw_heap_allocate(sizeof(zm_sound_t));
            sound->semitone_index = atoi(zm_mfr->tokens[1]);
            sound->velocity = atoi(zm_mfr->tokens[2]);
            sound->start = atoi(zm_mfr->tokens[3]);
            sound->duration = atoi(zm_mfr->tokens[4]);
            ysw_array_push(style->sounds, sound);
        } else {
            push_back_tokens(zm_mfr);
            done = true;
        }
    }

    ysw_array_push(zm_mfr->music->styles, style);
}

static void parse_pattern(zm_mfr_t *zm_mfr)
{
    zm_index_t index = atoi(zm_mfr->tokens[1]);
    zm_medium_t count = ysw_array_get_count(zm_mfr->music->patterns);

    if (index != count) {
        ESP_LOGW(TAG, "parse_pattern index=%d, count=%d", index, count);
        return;
    }

    zm_pattern_t *pattern = ysw_heap_allocate(sizeof(zm_pattern_t));
    pattern->name = ysw_heap_strdup(zm_mfr->tokens[2]);
    pattern->sample_index = atoi(zm_mfr->tokens[3]);
    pattern->steps = ysw_array_create(16);

    zm_yesno_t done = false;

    while (!done && get_tokens(zm_mfr)) {
        zm_mf_type_t type = atoi(zm_mfr->tokens[0]);
        if (type == ZM_MF_STEP && zm_mfr->token_count == 5) {
            zm_step_t *step = ysw_heap_allocate(sizeof(zm_step_t));
            step->root = atoi(zm_mfr->tokens[1]);
            step->quality = ysw_array_get(zm_mfr->music->qualities, atoi(zm_mfr->tokens[2]));
            step->style = ysw_array_get(zm_mfr->music->styles, atoi(zm_mfr->tokens[3]));
            step->duration = atoi(zm_mfr->tokens[4]);
            ysw_array_push(pattern->steps, step);
        } else {
            push_back_tokens(zm_mfr);
            done = true;
        }
    }

    ysw_array_push(zm_mfr->music->patterns, pattern);
}

static void parse_song(zm_mfr_t *zm_mfr)
{
    zm_index_t index = atoi(zm_mfr->tokens[1]);
    zm_medium_t count = ysw_array_get_count(zm_mfr->music->songs);

    if (index != count) {
        ESP_LOGW(TAG, "parse_song index=%d, count=%d", index, count);
        return;
    }

    zm_song_t *song = ysw_heap_allocate(sizeof(zm_song_t));
    song->name = ysw_heap_strdup(zm_mfr->tokens[2]);
    song->bpm = atoi(zm_mfr->tokens[3]);
    song->parts = ysw_array_create(16);

    zm_yesno_t done = false;

    while (!done && get_tokens(zm_mfr)) {
        zm_mf_type_t type = atoi(zm_mfr->tokens[0]);
        if (type == ZM_MF_PART && zm_mfr->token_count == 5) {
            zm_part_t *part = ysw_heap_allocate(sizeof(zm_part_t));
            part->pattern = ysw_array_get(zm_mfr->music->patterns, atoi(zm_mfr->tokens[1]));
            part->when.type = atoi(zm_mfr->tokens[2]);
            part->when.part_index = atoi(zm_mfr->tokens[3]);
            part->fit = atoi(zm_mfr->tokens[4]);
            ysw_array_push(song->parts, part);
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
    music->qualities = ysw_array_create(32);
    music->styles = ysw_array_create(64);
    music->patterns = ysw_array_create(64);
    music->songs = ysw_array_create(16);
    return music;
}

void zm_music_free(zm_music_t *music)
{
    zm_medium_t sample_count = ysw_array_get_count(music->samples);
    for (zm_medium_t i = 0; i < sample_count; i++) {
        zm_sample_t *sample = ysw_array_get(music->samples, i);
        ysw_heap_free(sample->name);
        ysw_heap_free(sample);
    }
    ysw_array_free(music->samples);

    zm_medium_t quality_count = ysw_array_get_count(music->qualities);
    for (zm_medium_t i = 0; i < quality_count; i++) {
        zm_quality_t *quality = ysw_array_get(music->qualities, i);
        ysw_heap_free(quality->name);
        ysw_array_free(quality->semitones);
    }
    ysw_array_free(music->qualities);

    zm_medium_t style_count = ysw_array_get_count(music->styles);
    for (zm_medium_t i = 0; i < style_count; i++) {
        zm_style_t *style = ysw_array_get(music->styles, i);
        ysw_heap_free(style->name);
        ysw_array_free(style->sounds);
    }
    ysw_array_free(music->styles);

    zm_medium_t pattern_count = ysw_array_get_count(music->patterns);
    for (zm_medium_t i = 0; i < pattern_count; i++) {
        zm_pattern_t *pattern = ysw_array_get(music->patterns, i);
        ysw_heap_free(pattern->name);
        ysw_array_free(pattern->steps);
    }
    ysw_array_free(music->patterns);

    zm_medium_t song_count = ysw_array_get_count(music->songs);
    for (zm_medium_t i = 0; i < song_count; i++) {
        zm_song_t *song = ysw_array_get(music->songs, i);
        ysw_heap_free(song->name);
        ysw_array_free(song->parts);
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
        } else if (type == ZM_MF_QUALITY && zm_mfr->token_count > 3) {
            parse_quality(zm_mfr);
        } else if (type == ZM_MF_STYLE && zm_mfr->token_count == 3) {
            parse_style(zm_mfr);
        } else if (type == ZM_MF_PATTERN && zm_mfr->token_count == 4) {
            parse_pattern(zm_mfr);
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

#define ZM_TPU YSW_TICKS_DEFAULT_TPM // ticks per unit

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
                        delta = left_note->instrument - right_note->instrument;
                    }
                }
            }
        }
    }
    return delta;
}

zm_large_t zm_render_pattern(ysw_array_t *notes, zm_pattern_t *pattern, zm_large_t start_time, zm_small_t channel)
{
    zm_large_t step_start = start_time;
    zm_medium_t step_count = ysw_array_get_count(pattern->steps);
    for (zm_medium_t i = 0; i < step_count; i++) {
        zm_step_t *step = ysw_array_get(pattern->steps, i);
        if (step->root) { // root == 0 is a rest
            zm_medium_t semitone_count = ysw_array_get_count(step->quality->semitones);
            zm_medium_t sound_count = ysw_array_get_count(step->style->sounds);
            for (zm_medium_t j = 0; j < sound_count; j++) {
                zm_sound_t *sound = ysw_array_get(step->style->sounds, j);
                if (sound->semitone_index >= semitone_count) {
                    ESP_LOGW(TAG, "semitone_index=%d, semitone_count=%d", sound->semitone_index, semitone_count);
                    continue;
                } 
                zm_semitone_t semitone = (uintptr_t)ysw_array_get(step->quality->semitones, sound->semitone_index);
                ysw_note_t *note = ysw_heap_allocate(sizeof(ysw_note_t));
                note->channel = channel;
                note->midi_note = step->root + semitone;
                note->start = step_start + (sound->start * step->duration) / ZM_TPU;
                note->duration = (sound->duration * step->duration) / ZM_TPU;
                note->velocity = sound->velocity;
                note->instrument = pattern->sample_index;
                ysw_array_push(notes, note);
                ESP_LOGD(TAG, "step_start=%d, midi_note=%d, start=%d, duration=%d, velocity=%d, channel=%d, sample=%d", step_start, note->midi_note, note->start, note->duration, note->velocity, note->channel, note->instrument);
            }
        }
        step_start += step->duration;
    }
    ESP_LOGD(TAG, "note_count=%d, end_time=%d", ysw_array_get_count(notes), step_start);
    return step_start;
}

ysw_array_t *zm_render_song(zm_song_t *song)
{
    zm_large_t max_time = 0;
    ysw_array_t *notes = ysw_array_create(512);
    ysw_array_t *part_times = ysw_array_create(8);
    zm_medium_t part_count = ysw_array_get_count(song->parts);
    for (zm_medium_t i = 0; i < part_count; i++) {
        zm_large_t begin_time = 0;
        zm_large_t end_time = 0;
        zm_part_t *part = ysw_array_get(song->parts, i);
        if (i == part->when.part_index) {
            begin_time = max_time;
            end_time = zm_render_pattern(notes, part->pattern, begin_time, i);
        } else if (part->when.type == ZM_WHEN_TYPE_WITH) {
            zm_part_time_t *part_time = ysw_array_get(part_times, part->when.part_index);
            begin_time = part_time->begin_time;
            if (part->fit == ZM_FIT_LOOP) {
                zm_large_t loop_time = begin_time;
                while (end_time < part_time->end_time) {
                    end_time = zm_render_pattern(notes, part->pattern, loop_time, i);
                    loop_time = end_time;
                } 
            } else {
                end_time = zm_render_pattern(notes, part->pattern, begin_time, i);
            }
        } else if (part->when.type == ZM_WHEN_TYPE_AFTER) {
            zm_part_time_t *part_time = ysw_array_get(part_times, part->when.part_index);
            begin_time = part_time->end_time;
            end_time = zm_render_pattern(notes, part->pattern, begin_time, i);
        }
        zm_part_time_t *part_time = ysw_heap_allocate(sizeof(zm_part_time_t));
        part_time->begin_time = begin_time;
        part_time->end_time = end_time;
        ysw_array_push(part_times, part_time);
        max_time = max(max_time, end_time);
    }
    ysw_array_free_all(part_times);
    ysw_array_sort(notes, zm_note_compare);
    ESP_LOGD(TAG, "note_count=%d", ysw_array_get_count(notes));
    return notes;
}

