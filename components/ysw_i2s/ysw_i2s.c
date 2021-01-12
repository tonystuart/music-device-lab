// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_i2s.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/dac.h"
#include "driver/i2s.h"

#define TAG "YSW_I2S"

typedef struct {
    ysw_i2s_generate_audio_cb_t generate_audio;
    ysw_i2s_initialize_cb_t initialize_i2s;
} ysw_i2s_t;

static void run_synth(void* context)
{
    ysw_i2s_t *i2s = context;

    i2s->initialize_i2s();

    for (;;) {
        uint8_t buf[YSW_I2S_BUFFER_SIZE];
        uint32_t bytes_generated = i2s->generate_audio(buf, sizeof(buf));
        size_t write_count = 0;
        $(i2s_write(I2S_NUM_0, buf, bytes_generated, &write_count, portMAX_DELAY));
    }
}

void ysw_i2s_create_task(ysw_i2s_initialize_cb_t initialize_i2s, ysw_i2s_generate_audio_cb_t generate_audio)
{
    ysw_i2s_t *i2s = ysw_heap_allocate(sizeof(ysw_i2s_t));
    i2s->generate_audio = generate_audio;
    i2s->initialize_i2s = initialize_i2s;

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.function = run_synth;
    config.context = i2s;

    ysw_task_create(&config);
}
