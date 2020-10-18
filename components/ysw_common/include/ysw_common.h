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
#include "stdbool.h"
#include "string.h"

#define RC_OK 0
#define RC_ERR (-1)

#define EOS 0 /* end of string null terminator */
#define RFT 1 /* room for terminator */

#define N(x) (x ? x : "null")

#define SMBUF_SZ 32
#define MDBUF_SZ 64
#define LGBUF_SZ 128

#define YSW_COMMON_FS_NAME_MAX 32 // SPIFFS limit

#define LOGIC_LOW 0
#define LOGIC_HIGH 1

#define UNUSED __attribute__ ((unused)) 
#define PACKED __attribute__((__packed__))

#define $ ESP_ERROR_CHECK

#define YSW_INT_PTR (void*)(uintptr_t)
#define YSW_PTR_INT (uintptr_t)(void*)

static inline int min(int x, int y)
{
    return x < y ? x : y;
}

static inline int max(int x, int y)
{
    return x > y ? x : y;
}

static inline int ysw_enforce_bounds(int x, int first, int last)
{
    return x < first ? first : x > last ? last : x;
}

static inline int ysw_to_count(int index)
{
    return index + 1;
}

static inline int ysw_to_index(int count)
{
    return count - 1;
}

static inline bool ysw_is_match(char *left, char *right)
{
    return left && right ? strcmp(left, right) == 0 : 0;
}

static inline uint32_t ysw_common_muldiv(int32_t multiplicand, int32_t multiplier, int32_t divisor)
{
    return (multiplicand * multiplier) / divisor;
}

uint32_t ysw_get_millis();
void ysw_wait_millis(int millis);
uint32_t ysw_ticks_to_millis(uint32_t ticks);
uint32_t ysw_millis_to_ticks(uint32_t millis);

char *ysw_itoa(int input_value, char *buffer, int buffer_size);
void ysw_copy(char *destination, const char* source, size_t size);
