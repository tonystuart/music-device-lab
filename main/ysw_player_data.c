// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_player_data.h"

#include "assert.h"
#include "esp_log.h"
#include "hash.h"
#include "ysw_heap.h"
#include "ysw_music.h"
#include "ysw_csv.h"

#define TAG "YSW_PLAYER_DATA"

#define RECORD_SIZE 128
#define TOKENS_SIZE 20

// Hash Types and Functions
// hash_t *id_address_map;
// void hash_ensure_assert_off();
// hash_alloc_insert(this->id_address_map, id, chord);
// this->id_address_map = hash_create(100, NULL, NULL);
// hash_free_nodes(this->id_address_map);
// hash_destroy(this->id_address_map);

typedef enum {
    CHORD = 1,
    CHORD_NOTE,
    PROGRESSION,
} record_type_t;

typedef struct {
    FILE *file;
    int record_count;
    int token_count;
    char buffer[RECORD_SIZE];
    char *tokens[TOKENS_SIZE];
    ysw_array_t *addresses;
    bool reuse_tokens;
} this_t;

static int get_tokens(this_t *this)
{
    if (this->reuse_tokens) {
        this->reuse_tokens = false;
        return true;
    }

    bool done = false;

    while (fgets(this->buffer, RECORD_SIZE, this->file) && !done) {
        ESP_LOGD(TAG, "record=%d, buffer=%s", this->record_count, this->buffer);
        this->token_count = parse_csv(this->buffer, this->tokens, TOKENS_SIZE);
        for (int i = 0; i < this->token_count; i++) {
            ESP_LOGD(TAG, "token[%d]=%s", i, this->tokens[i]);
        }
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

static ysw_chord_t *parse_chord(this_t *this)
{
    int id = atoi(this->tokens[1]);
    char *name = this->tokens[2];
    int duration = atoi(this->tokens[3]);
    ysw_chord_t *chord = ysw_chord_create(name, duration);
    int index = ysw_array_push(this->addresses, chord);
    assert(index == id);

    bool done = false;

    while (get_tokens(this) && !done) {
        record_type_t type = atoi(this->tokens[0]);
        if (type == CHORD_NOTE) {
            int degree = atoi(this->tokens[1]);
            int velocity = atoi(this->tokens[2]);
            int time = atoi(this->tokens[3]);
            int duration = atoi(this->tokens[4]);
            ysw_chord_note_t *note = ysw_chord_note_create(degree, velocity, time, duration);
            ysw_chord_add_note(chord, note);
        } else {
            push_back_tokens(this);
            done = true;
        }
    }

    return chord;
}

static ysw_player_data_t *create_player_data()
{
    ysw_player_data_t *player_data = ysw_heap_allocate(sizeof(ysw_player_data_t));
    player_data->chords = ysw_array_create(8);
    return player_data;
}

void ysw_player_data_free(ysw_player_data_t *player_data)
{
}

ysw_player_data_t *ysw_player_data_parse_file(FILE *file)
{
    this_t *this = &(this_t){};
    ysw_player_data_t *player_data = create_player_data();
    this->addresses = ysw_array_create(100);
    this->file = file;
    while (get_tokens(this)) {
        record_type_t type = atoi(this->tokens[0]);
        if (type == CHORD && this->token_count == 4) {
            ysw_chord_t *chord = parse_chord(this);
            ysw_array_push(player_data->chords, chord);
        } else if (type == PROGRESSION && this->token_count == 6) {
        }
    }
    ysw_array_free(this->addresses);
    return player_data;
}

ysw_player_data_t *ysw_player_data_parse(char *filename)
{
    ysw_player_data_t *player_data = NULL;
    FILE *file = fopen(filename, "r");
    if (file) {
        player_data = ysw_player_data_parse_file(file);
        fclose(file);
    }
    return player_data;
}

