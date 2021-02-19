// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_array.h"
#include "stdint.h"

typedef struct {
    uint32_t length;
    uint32_t size;
    char *buffer;
} ysw_string_t;

void ysw_string_append(ysw_string_t *s, ysw_string_t *value);
void ysw_string_append_char(ysw_string_t *s, char c);
void ysw_string_append_chars(ysw_string_t *s, const char *p);
ysw_string_t *ysw_string_create(uint32_t size);
void ysw_string_free(ysw_string_t *s);
uint32_t ysw_string_get_length(ysw_string_t *s);
char ysw_string_get_char_at(ysw_string_t *s, uint32_t index);
const char *ysw_string_get_chars(ysw_string_t *s);
ysw_string_t *ysw_string_init(const char *value);
ysw_array_t *ysw_string_parse(const char *source, const char *delimiter);
void ysw_string_printf(ysw_string_t *s, const char *template, ...);
void ysw_string_set(ysw_string_t *s, ysw_string_t *value);
void ysw_string_set_char(ysw_string_t *s, char c);
void ysw_string_set_chars(ysw_string_t *s, const char *value);
void ysw_string_shift(ysw_string_t *s, uint32_t index, int32_t amount);
void ysw_string_shorten(ysw_string_t *s, uint32_t max_length);
ysw_string_t *ysw_string_substring(ysw_string_t *source, uint32_t begin_index, uint32_t end_index);
void ysw_string_trim_left(ysw_string_t *source, const char *unwanted);
void ysw_string_trim_right(ysw_string_t *source, const char *unwanted);
void ysw_string_trim(ysw_string_t *source, const char *unwanted);
void ysw_string_strip(ysw_string_t *s, const char *unwanted);
