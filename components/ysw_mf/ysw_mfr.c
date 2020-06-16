// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_mfr.h"

#include "ysw_csv.h"
#include "ysw_heap.h"
#include "ysw_mf.h"
#include "ysw_music.h"

#include "esp_log.h"

#include "assert.h"
#include "errno.h"
#include "setjmp.h"
#include "stdlib.h"

#define TAG "YSW_MFR"

#define RECORD_SIZE 128
#define TOKENS_SIZE 20

typedef struct {
    FILE *file;
    uint32_t record_count;
    char buffer[RECORD_SIZE];
    char *tokens[TOKENS_SIZE];
    ysw_music_t *music;
    uint8_t token_count;
    bool reuse_tokens;
} ysw_mfr_t;

#define V(args...) \
    if (!(args)) { \
        ESP_LOGW(TAG, "validation failed on '" #args "', file=%s, line=%d", __FILE__, __LINE__);\
        longjmp(error_return, 1);\
    }

static jmp_buf error_return;

static int get_tokens(ysw_mfr_t *ysw_mfr)
{
    if (ysw_mfr->reuse_tokens) {
        ysw_mfr->reuse_tokens = false;
        return true;
    }

    bool done = false;

    // Read until we find a non-empty line or end of file
    while (!done && fgets(ysw_mfr->buffer, RECORD_SIZE, ysw_mfr->file)) {
        ysw_mfr->token_count = ysw_csv_parse(ysw_mfr->buffer, ysw_mfr->tokens, TOKENS_SIZE);
        ysw_mfr->record_count++;
        if (ysw_mfr->token_count > 0) {
            done = true;
        }
    }

    return done;
}

static void push_back_tokens(ysw_mfr_t *ysw_mfr)
{
    assert(!ysw_mfr->reuse_tokens);
    ysw_mfr->reuse_tokens = true;
}

static void parse_cs(ysw_mfr_t *ysw_mfr)
{
    uint32_t index = atoi(ysw_mfr->tokens[1]);
    uint32_t count = ysw_array_get_count(ysw_mfr->music->cs_array);

    if (index != count) {
        ESP_LOGW(TAG, "parse_cord expected count=%d, got index=%d", count, index);
        return;
    }

    ysw_cs_t *cs = ysw_cs_create(
        ysw_mfr->tokens[2],
        atoi(ysw_mfr->tokens[3]),
        atoi(ysw_mfr->tokens[4]),
        atoi(ysw_mfr->tokens[5]),
        atoi(ysw_mfr->tokens[6]),
        atoi(ysw_mfr->tokens[7]),
        atoi(ysw_mfr->tokens[8]));

    ysw_array_push(ysw_mfr->music->cs_array, cs);

    bool done = false;

    while (!done && get_tokens(ysw_mfr)) {
        ysw_mf_type_t type = atoi(ysw_mfr->tokens[0]);
        if (type == YSW_MF_STYLE_NOTE && ysw_mfr->token_count == 6) {
            int8_t degree = atoi(ysw_mfr->tokens[1]);
            uint8_t velocity = atoi(ysw_mfr->tokens[2]);
            uint32_t start = atoi(ysw_mfr->tokens[3]);
            uint32_t duration = atoi(ysw_mfr->tokens[4]);
            int8_t flags = atoi(ysw_mfr->tokens[5]);
            ysw_sn_t *sn = ysw_sn_create(degree, velocity, start, duration, flags);
            ysw_sn_normalize(sn);
            ysw_cs_add_sn(cs, sn);
        } else {
            push_back_tokens(ysw_mfr);
            done = true;
        }
    }
}

static void parse_hp(ysw_mfr_t *ysw_mfr)
{
    uint32_t index = atoi(ysw_mfr->tokens[1]);
    char *name = ysw_mfr->tokens[2];
    uint8_t tonic = atoi(ysw_mfr->tokens[3]);
    uint8_t instrument = atoi(ysw_mfr->tokens[4]);

    ESP_LOGD(TAG, "parse_hp index=%d, name=%s, tonic=%d, instrument=%d, record_count=%d", index, name, tonic, instrument, ysw_mfr->record_count);

    uint32_t count = ysw_array_get_count(ysw_mfr->music->hp_array);
    if (index != count) {
        ESP_LOGW(TAG, "parse_hp index=%d, count=%d", index, count);
        return;
    }

    ysw_hp_t *hp = ysw_hp_create(
        ysw_mfr->tokens[2],
        atoi(ysw_mfr->tokens[3]),
        atoi(ysw_mfr->tokens[4]),
        atoi(ysw_mfr->tokens[5]),
        atoi(ysw_mfr->tokens[6]),
        atoi(ysw_mfr->tokens[7]));
    ysw_array_push(ysw_mfr->music->hp_array, hp);

    bool done = false;

    while (!done && get_tokens(ysw_mfr)) {
        ysw_mf_type_t type = atoi(ysw_mfr->tokens[0]);
        if (type == YSW_MF_PROGRESSION_STEP && ysw_mfr->token_count == 4) {
            uint8_t degree = atoi(ysw_mfr->tokens[1]);
            uint32_t cs_index = atoi(ysw_mfr->tokens[2]);
            uint8_t flags = atoi(ysw_mfr->tokens[3]);
            uint32_t cs_count = ysw_array_get_count(ysw_mfr->music->cs_array);
            if (cs_index >= cs_count) {
                ESP_LOGW(TAG, "parse_hp cs_index=%d, cs_count=%d", cs_index, cs_count);
                ysw_hp_free(hp);
                return;
            }
            ysw_cs_t *cs = ysw_array_get(ysw_mfr->music->cs_array, cs_index);
            //ESP_LOGD(TAG, "using index=%d, cs=%p, name=%s", cs_index, cs, cs->name);
            ysw_hp_add_cs(hp, degree, cs, flags);
        } else {
            push_back_tokens(ysw_mfr);
            done = true;
        }
    }

    //ysw_hp_dump(hp, TAG);
}

static ysw_music_t *create_music()
{
    ysw_music_t *music = ysw_heap_allocate(sizeof(ysw_music_t));
    music->cs_array = ysw_array_create(64);
    music->hp_array = ysw_array_create(64);
    return music;
}

void ysw_mfr_free(ysw_music_t *music)
{
    uint32_t cs_count = ysw_array_get_count(music->cs_array);
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_cs_t *cs = ysw_array_get(music->cs_array, i);
        ysw_cs_free(cs);
    }
    ysw_array_free(music->cs_array);
    uint32_t hp_count = ysw_array_get_count(music->hp_array);
    for (uint32_t i = 0; i < hp_count; i++) {
        ysw_hp_t *hp = ysw_array_get(music->hp_array, i);
        ysw_hp_free(hp);
    }
    ysw_array_free(music->hp_array);
}

ysw_music_t *ysw_mfr_read_from_file(FILE *file)
{
    ysw_mfr_t *ysw_mfr = &(ysw_mfr_t){};

    if (setjmp(error_return)) {
        ESP_LOGW(TAG, "caught parser exception");
        if (ysw_mfr->music) {
            ysw_mfr_free(ysw_mfr->music);
        }
        return NULL;
    }

    ysw_mfr->file = file;
    ysw_mfr->music = create_music();

    while (get_tokens(ysw_mfr)) {
        ysw_mf_type_t type = atoi(ysw_mfr->tokens[0]);
        if (type == YSW_MF_CHORD_STYLE && ysw_mfr->token_count == 9) {
            parse_cs(ysw_mfr);
        } else if (type == YSW_MF_HARMONIC_PROGRESSION && ysw_mfr->token_count == 8) {
            parse_hp(ysw_mfr);
        } else {
            ESP_LOGW(TAG, "invalid record type=%d, token_count=%d", type, ysw_mfr->token_count);
            for (uint32_t i = 0; i < ysw_mfr->token_count; i++) {
                ESP_LOGW(TAG, "token[%d]=%s", i, ysw_mfr->tokens[i]);
            }
        }
    }

    return ysw_mfr->music;
}

ysw_music_t *ysw_mfr_read()
{
    ysw_music_t *music = NULL;
    FILE *file = fopen(YSW_MUSIC_CSV, "r");
    if (!file) {
        // spiffs doesn't provide atomic rename, so check for temp file
        ESP_LOGE(TAG, "fopen file=%s failed, errno=%d", YSW_MUSIC_CSV, errno);
        int rc = rename(YSW_MUSIC_TEMP, YSW_MUSIC_CSV);
        if (rc == -1) {
            ESP_LOGE(TAG, "rename old=%s new=%s failed, errno=%d", YSW_MUSIC_TEMP, YSW_MUSIC_CSV, errno);
            abort();
        }
        file = fopen(YSW_MUSIC_CSV, "r");
        ESP_LOGE(TAG, "fopen file=%s failed (after rename), errno=%d", YSW_MUSIC_CSV, errno);
        abort();
    }
    music = ysw_mfr_read_from_file(file);
    fclose(file);
    return music;
}

