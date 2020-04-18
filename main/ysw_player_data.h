// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "stdio.h"
#include "assert.h"
#include "esp_log.h"
#include "hash.h"
#include "ysw_heap.h"
#include "ysw_clip.h"
#include "ysw_csv.h"

typedef struct {
    ysw_array_t *chords;
} ysw_player_data_t;

ysw_player_data_t *ysw_player_data_parse(char *filename);
