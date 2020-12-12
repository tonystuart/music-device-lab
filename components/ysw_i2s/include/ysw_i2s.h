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

typedef int32_t (*ysw_i2s_generate_audio_t)(uint8_t *buffer, int32_t bytes_requested);

void ysw_i2s_create_task(ysw_i2s_generate_audio_t generate_audio);
