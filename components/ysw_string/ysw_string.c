// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_string.h"
#include "ysw_heap.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_STRING"

ysw_string_t *ysw_string_create(uint32_t size)
{
    ysw_string_t *s = ysw_heap_allocate(sizeof(ysw_string_t));
    s->buffer = ysw_heap_allocate(size);
    s->size = size;
    return s;
}

void ysw_string_append_char(ysw_string_t *s, uint8_t c)
{
    if (s->length + 2 >= s->size) {
        s->size *= 2;
        s->buffer = ysw_heap_reallocate(s->buffer, s->size);
    }
    s->length++;
    s->buffer[s->length - 2] = c;
    s->buffer[s->length - 1] = 0;
}

uint32_t ysw_string_get_length(ysw_string_t *s)
{
    return s->length;
}

uint8_t ysw_string_get_char_at(ysw_string_t *s, uint32_t index)
{
    assert(index < s->length);
    return s->buffer[index];
}

const uint8_t *ysw_string_get_chars(ysw_string_t *s)
{
    return s->buffer;
}
