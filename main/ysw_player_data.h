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
#include "ysw_array.h"

typedef struct {
    ysw_array_t *chords;
} ysw_player_data_t;

ysw_player_data_t *ysw_player_data_parse(char *filename);
ysw_player_data_t *ysw_player_data_parse_file(FILE *file);
