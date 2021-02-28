// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_csv.h"
#include "ysw_heap.h"
#include "assert.h"
#include "stdbool.h"
#include "stdint.h"

#define TAG "YSW_CSV"

int ysw_csv_parse(char *buffer, char *tokens[], int max_tokens)
{
    assert(buffer);
    assert(tokens);
    assert(max_tokens > 0);

    typedef enum {
        INITIAL,
        ESCAPE,
    } parser_state_t;

    char *p = buffer;
    char *q = buffer;
    int token_index = 0;
    tokens[token_index] = q;
    parser_state_t state = INITIAL;
    bool scanning = true;

    while (scanning) {
        switch (state) {
            case INITIAL:
                if (!*p || *p == '#' || *p == '\n') {
                    scanning = false;
                } else if (*p == '\\') {
                    p++;
                    state = ESCAPE;
                } else if (*p == ',') {
                    p++;
                    *q++ = 0;
                    if (token_index < (max_tokens - 1)) {
                        tokens[++token_index] = q;
                    } else {
                        scanning = false;
                    }
                } else {
                    *q++ = *p++;
                }
                break;
            case ESCAPE:
                if (!*p) {
                    scanning = false;
                } else if (*p == '\n') {
                    // skip newline
                    p++;
                } else {
                    // preserve everything else (e.g. backslash, comma, pound sign)
                    *q++ = *p++;
                }
                state = INITIAL;
                break;
        }
    }

    *q = 0;
    return *tokens[0] ? token_index + 1 : 0;
}

#define ROOM_TO_EXPAND 2 // leave room for escape character and end-of-string

void ysw_csv_escape(const char *source, char *target, int target_size)
{
    assert(target_size > ROOM_TO_EXPAND);

    const char *s = source;
    char *t = target;
    char *t_max = target + target_size - ROOM_TO_EXPAND;

    while (*s && t < t_max) {
        if (*s == '\\' || *s == ',' || *s == '#' || *s == '\n') {
            *t++ = '\\';
        }
        *t++ = *s++;
    }

    *t = 0;
}

uint32_t ysw_csv_parse_next_record(ysw_csv_t *csv)
{
    assert(csv);

    if (csv->is_pushback) {
        csv->is_pushback = false;
    } else {
        csv->token_count = 0;
        while (!csv->token_count && fgets(csv->buffer, csv->record_size, csv->file)) {
            csv->token_count = ysw_csv_parse(csv->buffer, csv->tokens, csv->tokens_size);
            csv->record_count++;
        }
    }
    return csv->token_count;
}

void ysw_csv_push_back_record(ysw_csv_t *csv)
{
    assert(csv);
    assert(!csv->is_pushback);

    csv->is_pushback = true;
}

int32_t ysw_csv_get_token_count(ysw_csv_t *csv)
{
    assert(csv);

    return csv->token_count;
}

int32_t ysw_csv_get_record_count(ysw_csv_t *csv)
{
    assert(csv);

    return csv->record_count;
}

char *ysw_csv_get_token(ysw_csv_t *csv, uint32_t index)
{
    assert(csv);
    assert(index < csv->tokens_size);

    return csv->tokens[index];
}

ysw_csv_t *ysw_csv_create(FILE *file, uint32_t record_size, uint32_t tokens_size)
{
    assert(file);
    assert(record_size);
    assert(tokens_size);

    ysw_csv_t *csv = ysw_heap_allocate(sizeof(ysw_csv_t));
    csv->buffer = ysw_heap_allocate(sizeof(char) *record_size);
    csv->tokens = ysw_heap_allocate(sizeof(char *) *tokens_size);
    csv->file = file;
    csv->record_size = record_size;
    csv->tokens_size = tokens_size;
    return csv;
}

void ysw_csv_free(ysw_csv_t *csv)
{
    assert(csv);
    assert(csv->tokens);
    assert(csv->buffer);

    ysw_heap_free(csv->tokens);
    ysw_heap_free(csv->buffer);
    ysw_heap_free(csv);
}
