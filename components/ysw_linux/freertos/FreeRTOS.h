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

#define pdTRUE true
#define pdFALSE false
#define portMAX_DELAY (-1)
#define portTICK_PERIOD_MS 1
#define configSTACK_DEPTH_TYPE uint32_t
#define errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY (-1)
#define pdPASS pdTRUE

typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef int TickType_t;

