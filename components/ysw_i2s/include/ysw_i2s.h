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

#define YSW_I2S_SAMPLE_RATE 44100.0
#define YSW_I2S_BUFFER_COUNT 2
#define YSW_I2S_SAMPLE_COUNT 128
#define YSW_I2S_CHANNEL_COUNT 2
#define YSW_I2S_SAMPLE_SIZE (sizeof(uint16_t))
#define YSW_I2S_BUFFER_SIZE (YSW_I2S_SAMPLE_COUNT * YSW_I2S_SAMPLE_SIZE * YSW_I2S_CHANNEL_COUNT)

typedef void (*ysw_i2s_initialize_cb_t)();

typedef int32_t (*ysw_i2s_generate_audio_cb_t)(uint8_t *buffer, int32_t bytes_requested);

void ysw_i2s_create_task(ysw_i2s_initialize_cb_t initialize_i2s, ysw_i2s_generate_audio_cb_t generate_audio);
