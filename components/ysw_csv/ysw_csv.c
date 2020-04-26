// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_csv.h"

#include "stdbool.h"

#define TAG "YSW_CSV"

int ysw_csv_parse(char *buffer, char *tokens[], int max_tokens)
{
    typedef enum {
        INITIAL,
        ESCAPE,
        ASSIGN
    } parser_state_t;

    char *p = buffer;
    char *q = buffer;
    int token_count = 0;
    bool new_token = true;
    bool scanning = true;
    parser_state_t state = INITIAL;

    while (scanning) {
        //ESP_LOGD(TAG, "*p=%c, *q=%c, token_count=%d, new_token=%d, scanning=%d, state=%d", *p, *q, token_count, new_token, scanning, state);
        switch (state) {
            case INITIAL:
                if (!*p || *p == '#' || *p == '\n') {
                    scanning = false;
                } else if (*p == '\\') {
                    p++;
                    state = ESCAPE;
                } else {
                    state = ASSIGN;
                }
                break;
            case ESCAPE:
                if (!*p) {
                    scanning = false;
                } else if (*p == '\n') {
                    // skip newline
                    p++;
                } else {
                    state = ASSIGN;
                }
                break;
            case ASSIGN:
                if (new_token) {
                    if (token_count < max_tokens) {
                        tokens[token_count++] = q;
                        new_token = false;
                    } else {
                        scanning = false;
                    }
                }
                if (*p == ',') {
                    *p = 0;
                    new_token = true;
                }
                *q++ = *p++;
                state = INITIAL;
                break;
        }
    }
    *q = 0;
    return token_count;
}

