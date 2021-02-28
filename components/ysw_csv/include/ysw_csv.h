// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"

typedef struct {
    FILE *file;
    char *buffer;
    char **tokens;
    uint32_t record_size;
    uint32_t tokens_size;
    uint32_t token_count;
    uint32_t record_count;
    bool is_pushback;
} ysw_csv_t;

ysw_csv_t *ysw_csv_create(FILE *file, uint32_t record_size, uint32_t tokens_size);
int ysw_csv_parse(char *buffer, char *tokens[], int max_tokens);
uint32_t ysw_csv_parse_next_record(ysw_csv_t *csv);
void ysw_csv_push_back_record(ysw_csv_t *csv);
int32_t ysw_csv_get_token_count(ysw_csv_t *csv);
int32_t ysw_csv_get_record_count(ysw_csv_t *csv);
char *ysw_csv_get_token(ysw_csv_t *csv, uint32_t index);
void ysw_csv_escape(const char *source, char *target, int target_size);
void ysw_csv_free(ysw_csv_t *csv);
