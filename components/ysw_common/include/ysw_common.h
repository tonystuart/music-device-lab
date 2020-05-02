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

#define LOGIC_LOW 0
#define LOGIC_HIGH 1

#define UNUSED __attribute__ ((unused)) 
#define PACKED __attribute__((__packed__))

#define $ ESP_ERROR_CHECK

static inline int min(int x, int y)
{
    return x < y ? x : y;
}

static inline int max(int x, int y)
{
    return x > y ? x : y;
}

static inline int to_count(int index)
{
    return index + 1;
}

static inline int to_index(int count)
{
    return count - 1;
}

static inline bool match(char *left, char *right)
{
    return left && right ? strcmp(left, right) == 0 : 0;
}

uint32_t get_millis();
void wait_millis(int millis);
uint32_t to_millis(uint32_t ticks);
uint32_t to_ticks(uint32_t millis);

char *ysw_itoa(int input_value, char *buffer, int buffer_size);
void ysw_copy(char *destination, const char* source, size_t size);
