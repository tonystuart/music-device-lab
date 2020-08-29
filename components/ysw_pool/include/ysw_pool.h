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

typedef void* ysw_pool_handle_t;

typedef enum {
    YSW_POOL_ACTION_STOP = 0x01,
    YSW_POOL_ACTION_FREE = 0x02,
} ysw_pool_action_t;

typedef ysw_pool_action_t (*ysw_pool_visitor_t)(void *context, uint32_t index, uint32_t count, void *item);
