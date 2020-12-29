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

static inline void *ysw_common_validate_pointer(void *p, char *file, int line, char *arg)
{
    if (!p) {
        extern void abort();
        extern int printf(const char *, ...);
        printf("%s:%d %s failed", file, line, arg);
        abort();
    }
    return p;
}

#define C(x) ysw_common_validate_pointer(x, __FILE__, __LINE__, #x)

#define YSW_PTR (void*)(uintptr_t)
#define YSW_INT (uintptr_t)(void*)

// TODO: rename ysw_int32_min to avoid inadvertent use with uint32_t

static inline int32_t min(int32_t x, int32_t y)
{
    return x < y ? x : y;
}

// TODO: rename ysw_int32_max to avoid inadvertent use with uint32_t

static inline int32_t max(int32_t x, int32_t y)
{
    return x > y ? x : y;
}

static inline uint32_t ysw_uint32_min(uint32_t x, uint32_t y)
{
    return x < y ? x : y;
}

static inline uint32_t ysw_uint32_max(uint32_t x, uint32_t y)
{
    return x > y ? x : y;
}

static inline uint32_t ysw_enforce_bounds(int x, int first, int last)
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
uint32_t ysw_rtos_ticks_to_millis(uint32_t ticks);
uint32_t ysw_millis_to_rtos_ticks(uint32_t millis);
char *ysw_replace(const char *source, const char *from, const char *to);
char *ysw_make_label(const char *name);

char *ysw_itoa(int input_value, char *buffer, int buffer_size);
void ysw_copy(char *destination, const char* source, size_t size);
