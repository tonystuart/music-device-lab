// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_csv.h"

#include "assert.h"
#include "stdbool.h"
#include "stdint.h"

#define TAG "YSW_CSV"

int ysw_csv_parse(char *buffer, char *tokens[], int max_tokens)
{
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
    return *tokens[token_index] ? token_index + 1 : token_index;
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

