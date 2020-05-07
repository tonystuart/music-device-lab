// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_music_parser.h"

#include "setjmp.h"
#include "assert.h"
#include "esp_log.h"
#include "hash.h"
#include "ysw_heap.h"
#include "ysw_music.h"
#include "ysw_csv.h"

#define TAG "YSW_MUSIC_PARSER"

#define RECORD_SIZE 128
#define TOKENS_SIZE 20

// Hash Types and Functions
// hash_t *id_address_map;
// void hash_ensure_assert_off();
// hash_alloc_insert(this->id_address_map, id, cs);
// this->id_address_map = hash_create(100, NULL, NULL);
// hash_free_nodes(this->id_address_map);
// hash_destroy(this->id_address_map);

typedef enum {
    CHORD_STYLE = 1,
    CHORD_NOTE,
    PROGRESSION,
    PROGRESSION_CHORD_STYLE,
    MELODY,
    RHYTHM,
    MIX,
} record_type_t;

typedef struct {
    FILE *file;
    uint32_t record_count;
    char buffer[RECORD_SIZE];
    char *tokens[TOKENS_SIZE];
    ysw_music_t *music;
    uint8_t token_count;
    bool reuse_tokens;
} this_t;

#define V(args...) \
    if (!(args)) { \
        ESP_LOGW(TAG, "validation failed on '" #args "', file=%s, line=%d", __FILE__, __LINE__);\
        longjmp(error_return, 1);\
    }

static jmp_buf error_return;

static int get_tokens(this_t *this)
{
    if (this->reuse_tokens) {
        this->reuse_tokens = false;
        return true;
    }

    bool done = false;

    // Read until we find a non-empty line or end of file
    while (!done && fgets(this->buffer, RECORD_SIZE, this->file)) {
        this->token_count = ysw_csv_parse(this->buffer, this->tokens, TOKENS_SIZE);
        this->record_count++;
        if (this->token_count > 0) {
            done = true;
        }
    }

    return done;
}

static void push_back_tokens(this_t *this)
{
    assert(!this->reuse_tokens);
    this->reuse_tokens = true;
}

static void parse_cs(this_t *this)
{
    uint32_t index = atoi(this->tokens[1]);
    uint32_t count = ysw_array_get_count(this->music->css);

    if (index != count) {
        ESP_LOGW(TAG, "parse_cord expected count=%d, got index=%d", count, index);
        return;
    }

    ysw_cs_t *cs = ysw_cs_create(
        this->tokens[2],
        atoi(this->tokens[3]),
        atoi(this->tokens[4]),
        atoi(this->tokens[5]),
        atoi(this->tokens[6]),
        atoi(this->tokens[7]),
        atoi(this->tokens[8]));

    ysw_array_push(this->music->css, cs);

    bool done = false;

    while (!done && get_tokens(this)) {
        record_type_t type = atoi(this->tokens[0]);
        if (type == CHORD_NOTE && this->token_count == 6) {
            int8_t degree = atoi(this->tokens[1]);
            uint8_t velocity = atoi(this->tokens[2]);
            uint32_t start = atoi(this->tokens[3]);
            uint32_t duration = atoi(this->tokens[4]);
            int8_t flags = atoi(this->tokens[5]);
            ysw_csn_t *note = ysw_csn_create(degree, velocity, start, duration, flags);
            ysw_cs_add_csn(cs, note);
        } else {
            push_back_tokens(this);
            done = true;
        }
    }
}

static void parse_progression(this_t *this)
{
    uint32_t index = atoi(this->tokens[1]);
    char *name = this->tokens[2];
    uint8_t tonic = atoi(this->tokens[3]);
    uint8_t instrument = atoi(this->tokens[4]);

    ESP_LOGD(TAG, "parse_progression index=%d, name=%s, tonic=%d, instrument=%d, record_count=%d", index, name, tonic, instrument, this->record_count);

    uint32_t count = ysw_array_get_count(this->music->progressions);
    if (index != count) {
        ESP_LOGW(TAG, "parse_progression index=%d, count=%d", index, count);
        return;
    }

    ysw_progression_t *progression = ysw_progression_create(name, tonic, instrument);
    ysw_array_push(this->music->progressions, progression);

    bool done = false;

    while (!done && get_tokens(this)) {
        record_type_t type = atoi(this->tokens[0]);
        if (type == PROGRESSION_CHORD_STYLE && this->token_count == 3) {
            uint8_t degree = atoi(this->tokens[1]);
            uint32_t cs_index = atoi(this->tokens[2]);
            uint32_t cs_count = ysw_array_get_count(this->music->css);
            if (cs_index >= cs_count) {
                ESP_LOGW(TAG, "parse_progression cs_index=%d, cs_count=%d", cs_index, cs_count);
                ysw_progression_free(progression);
                return;
            }
            ysw_cs_t *cs = ysw_array_get(this->music->css, cs_index);
            //ESP_LOGD(TAG, "using index=%d, cs=%p, name=%s", cs_index, cs, cs->name);
            ysw_progression_add_cs(progression, degree, cs);
        } else {
            push_back_tokens(this);
            done = true;
        }
    }

    //ysw_progression_dump(progression, TAG);
}

static ysw_music_t *create_music()
{
    ysw_music_t *music = ysw_heap_allocate(sizeof(ysw_music_t));
    music->css = ysw_array_create(64);
    music->progressions = ysw_array_create(64);
    return music;
}

void ysw_music_free(ysw_music_t *music)
{
    uint32_t cs_count = ysw_array_get_count(music->css);
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_cs_t *cs = ysw_array_get(music->css, i);
        ysw_cs_free(cs);
    }
    ysw_array_free(music->css);
    uint32_t progression_count = ysw_array_get_count(music->progressions);
    for (uint32_t i = 0; i < progression_count; i++) {
        ysw_progression_t *progression = ysw_array_get(music->progressions, i);
        ysw_progression_free(progression);
    }
    ysw_array_free(music->progressions);
}

ysw_music_t *ysw_music_parse_file(FILE *file)
{
    this_t *this = &(this_t){};

    if (setjmp(error_return)) {
        ESP_LOGW(TAG, "caught parser exception");
        if (this->music) {
            ysw_music_free(this->music);
        }
        return NULL;
    }

    this->file = file;
    this->music = create_music();

    while (get_tokens(this)) {
        record_type_t type = atoi(this->tokens[0]);
        if (type == CHORD_STYLE && this->token_count == 9) {
            parse_cs(this);
        } else if (type == PROGRESSION && this->token_count == 5) {
            parse_progression(this);
        } else {
            ESP_LOGW(TAG, "invalid record type=%d, token_count=%d", type, this->token_count);
            for (uint32_t i = 0; i < this->token_count; i++) {
                ESP_LOGW(TAG, "token[%d]=%s", i, this->tokens[i]);
            }
        }
    }

    return this->music;
}

ysw_music_t *ysw_music_parse(char *filename)
{
    ESP_LOGD(TAG, "ysw_music_parse filename=%s", filename);
    ysw_music_t *music = NULL;
    FILE *file = fopen(filename, "r");
    if (file) {
        music = ysw_music_parse_file(file);
        fclose(file);
    }
    return music;
}

