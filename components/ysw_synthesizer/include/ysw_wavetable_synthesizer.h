// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/dac.h"
#include "driver/i2s.h"
#include "ysw_common.h"
#include "ysw_midi.h"
#include "ysw_task.h"
#include "ysw_wavetable.h"
#include "ysw_synthesizer.h"

//#define SAMPLE_RATE 22050.0
#define SAMPLE_RATE 44100.0

QueueHandle_t ysw_wavetable_synthesizer_create_task(uint8_t dac_left_gpio, uint8_t dac_right_gpio);

