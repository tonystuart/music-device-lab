// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_string.h"
#include "ysw_common.h"
#include "ysw_heap.h"
#include "esp_log.h"
#include "assert.h"
#include "stdarg.h"
#include "stdbool.h"
#include "stdio.h"

#define TAG "YSW_STRING"

// TODO: consider relative merits of immutability versus minimizing heap allocations

static uint32_t fast_length(const char *value)
{
    const char *value_p = value;
    while (*value_p) {
        value_p++;
    }
    return value_p - value;
}

static void fast_copy(char *target, const char *source)
{
    char *target_p = target;
    const char *source_p = source;
    while (*source_p) {
        *target_p++ = *source_p++;
    }
}

static void ensure_capacity(ysw_string_t *s, uint32_t required)
{
    if (required > s->size) {
        s->size = s->size * ((2 * required - 1) / s->size);
        s->buffer = ysw_heap_reallocate(s->buffer, s->size);
    }
}

ysw_string_t *ysw_string_create(uint32_t size)
{
    assert(size);
    ysw_string_t *s = ysw_heap_allocate(sizeof(ysw_string_t));
    s->buffer = ysw_heap_allocate(size);
    s->size = size;
    return s;
}

ysw_string_t *ysw_string_init(const char *value)
{
    uint32_t value_length = fast_length(value);
    ysw_string_t *target = ysw_string_create(value_length + RFT);
    fast_copy(target->buffer, value);
    target->length = value_length;
    return target;
}

void ysw_string_append(ysw_string_t *s, ysw_string_t *value)
{
    ensure_capacity(s, s->length + value->length + RFT);
    fast_copy(s->buffer + s->length, value->buffer);
    s->length += value->length;
    s->buffer[s->length] = 0;
}

void ysw_string_set(ysw_string_t *s, ysw_string_t *value)
{
    s->length = 0;
    ysw_string_append(s, value);
}

void ysw_string_append_char(ysw_string_t *s, char c)
{
    ensure_capacity(s, s->length + 1 + RFT); // +1 for new character
    s->buffer[s->length++] = c;
    s->buffer[s->length] = 0;
}

void ysw_string_set_char(ysw_string_t *s, char c)
{
    s->length = 0;
    ysw_string_append_char(s, c);
}

void ysw_string_append_chars(ysw_string_t *s, const char *value)
{
    uint32_t value_length = fast_length(value);
    ensure_capacity(s, s->length + value_length + RFT);
    fast_copy(s->buffer + s->length, value);
    s->length = s->length + value_length;
    s->buffer[s->length] = 0;
}

void ysw_string_set_chars(ysw_string_t *s, const char *value)
{
    s->length = 0;
    ysw_string_append_chars(s, value);
}

ysw_string_t *ysw_string_substring(ysw_string_t *source, uint32_t begin_index, uint32_t end_index)
{
    uint32_t value_length = end_index - begin_index;
    ysw_string_t *target = ysw_string_create(value_length + RFT);
    memcpy(target->buffer, source->buffer + begin_index, value_length);
    target->length = value_length;
    target->buffer[target->length] = 0;
    return target;
}

void ysw_string_printf(ysw_string_t *s, const char *template, ...)
{
    va_list arguments;
    va_start(arguments, template);
    char *start = s->buffer + s->length;
    uint32_t available = s->size - s->length;
    uint32_t rc = vsnprintf(start, available, template, arguments);
    if (rc >= available) {
        ensure_capacity(s, rc + RFT);
        available = s->size - s->length;
        va_end(arguments);
        va_start(arguments, template);
        rc = vsnprintf(start, available, template, arguments);
    }
    s->length += rc;
    va_end(arguments);
}

uint32_t ysw_string_get_length(ysw_string_t *s)
{
    return s->length;
}

char ysw_string_get_char_at(ysw_string_t *s, uint32_t index)
{
    assert(index < s->length);
    return s->buffer[index];
}

const char *ysw_string_get_chars(ysw_string_t *s)
{
    return s->buffer;
}

static bool is_in(char c, const char *delimiters)
{
    const char *d = delimiters;
    while (*d) {
        if (c == *d) {
            return true;
        }
        d++;
    }
    return false;
}

ysw_array_t *ysw_string_parse(const char *source, const char *delimiters)
{
    const char *p = source;
    ysw_string_t *token = NULL;
    ysw_array_t *tokens = ysw_array_create(8);
    ysw_array_set_free_cb(tokens, (ysw_array_free_cb_t)ysw_string_free);
    while (*p) {
        if (is_in(*p, delimiters)) {
            token = NULL;
        } else {
            if (!token) {
                token = ysw_string_create(32);
                ysw_array_push(tokens, token);
            }
            ysw_string_append_char(token, *p);
        }
        p++;
    }
    return tokens;
}

void ysw_string_shift(ysw_string_t *s, uint32_t index, int32_t amount)
{
    assert(s);
    assert(index <= s->length);
    if (amount > 0) {
        ensure_capacity(s, s->length + amount + RFT);
        uint32_t to_copy = s->length - index;
        char *target_p = s->buffer + s->length + amount;
        char *source_p = s->buffer + s->length - 1; // because s->length is 1-based
        *target_p-- = EOS;
        for (uint32_t i = 0; i < to_copy; i++) {
            *target_p-- = *source_p--;
        }
        uint32_t to_insert = amount;
        for (uint32_t i = 0; i < to_insert; i++) {
            *target_p-- = ' ';
        }
        s->length += amount;
    } else if (amount < 0) {
        amount = -amount;
        assert(amount <= index);
        uint32_t to_copy = s->length - index - amount;
        char *target_p = s->buffer + index;
        char *source_p = s->buffer + index + amount;
        for (uint32_t i = 0; i < to_copy; i++) {
            *target_p++ = *source_p++;
        }
        *target_p = EOS;
        s->length -= amount;
    }
}

void ysw_string_shorten(ysw_string_t *s, uint32_t max_length)
{
    // Remove leading and trailing spaces
    ysw_string_trim(s, " ");

    // Remove ASCII vowels starting from right
    int32_t index = s->length - RFT;
    while (index > 0 && s->length > max_length) {
        if (is_in(s->buffer[index], "aeiouAEIOU")) {
            ysw_string_shift(s, index, -1);
        }
        index--;
    }
    // Remove every other consonant starting from right
    index = s->length - RFT;
    while (index > 1 && s->length > max_length) {
        ysw_string_shift(s, index, -1);
        index -= 2;
    }
    // Truncate to maxium length
    if (s->length > max_length) {
        s->length = max_length;
    }
}

void ysw_string_trim_left(ysw_string_t *s, const char *unwanted)
{
    char *p = s->buffer;
    while (*p && is_in(*p, unwanted)) {
        p++;
    }
    if (*p && p > s->buffer) {
        char *q = s->buffer;
        while (*p) {
            *q++ = *p++;
        }
        *q++ = EOS;
        s->length = q - s->buffer;
    }
}

void ysw_string_trim_right(ysw_string_t *s, const char *unwanted)
{
    char *p = s->buffer + s->length - RFT;
    while (p > s->buffer && is_in(*p, unwanted)) {
        p--;
    }
    p++;
    *p++ = EOS;
    s->length = p - s->buffer;
}

void ysw_string_strip(ysw_string_t *s, const char *unwanted)
{
    assert(s);
    char *p = s->buffer;
    char *q = p;
    while (*p) {
        if (is_in(*p, unwanted)) {
            p++;
        } else {
            *q++ = *p++;
        }
    }
    *q++ = EOS;
    s->length = q - s->buffer;
}

void ysw_string_trim(ysw_string_t *source, const char *unwanted)
{
    ysw_string_trim_left(source, unwanted);
    ysw_string_trim_right(source, unwanted);
}

void ysw_string_free(ysw_string_t *s)
{
    ysw_heap_free(s->buffer);
    ysw_heap_free(s);
}

