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

// Note that sizeof(intptr_t) == sizeof(uintptr_t) == sizeof(void*)

typedef union {
    bool b;
    int16_t ss;
    uint16_t us;
    int32_t si;
    uint32_t ui;
    void *p;
} ysw_value_t;

