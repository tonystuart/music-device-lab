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
#include "ysw_music.h"

void ysw_mfw_write(ysw_music_t *music);
void ysw_mfw_write_to_file(FILE *file, ysw_music_t *music);
