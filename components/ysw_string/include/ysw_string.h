// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "stdint.h"

typedef struct {
    uint32_t length;
    uint32_t size;
    uint8_t *buffer;
} ysw_string_t;

ysw_string_t *ysw_string_create(uint32_t size);
void ysw_string_append_char(ysw_string_t *s, uint8_t c);
uint32_t ysw_string_get_length(ysw_string_t *s);
uint8_t ysw_string_get_char_at(ysw_string_t *s, uint32_t index);
const uint8_t *ysw_string_get_chars(ysw_string_t *s);
