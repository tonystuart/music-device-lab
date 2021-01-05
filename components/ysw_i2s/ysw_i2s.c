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

#define SAMPLE_RATE 44100.0
#define BUFFER_COUNT 2
#define SAMPLE_COUNT 128
#define CHANNEL_COUNT 2
#define SAMPLE_SIZE (sizeof(uint16_t))
#define BUFFER_SIZE (SAMPLE_COUNT * SAMPLE_SIZE * CHANNEL_COUNT)

typedef struct {
    ysw_i2s_generate_audio_t generate_audio;
} ysw_i2s_t;

static void initialize(void)
{
#if  0
    // Enable I2S internal DAC on both GPIO 25 and GPIO 26
    ESP_LOGD(TAG, "Enabling I2S internal DAC on both GPIO 25 and GPIO 26");

    static const i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = 0,
        .dma_buf_count = BUFFER_COUNT,
        .dma_buf_len = SAMPLE_COUNT,
        .use_apll = false
    };

    $(i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL));

    $(i2s_set_pin(I2S_NUM_0, NULL));
#endif
#if 0
    // Enable I2S internal DAC on GPIO 25 with GPIO 26 in use for keyboard
    ESP_LOGD(TAG, "Enabling I2S internal DAC on GPIO 25 with GPIO 26 in use for keyboard");

    static const i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = 0,
        .dma_buf_count = BUFFER_COUNT,
        .dma_buf_len = SAMPLE_COUNT,
        .use_apll = false
    };

    $(i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL));

    ESP_LOGD(TAG, "calling i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN)");
    $(i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN));
#endif
#if 1
    // Enable I2S external Cirrus Logic CS4344 DAC using GPIO 0 for MCLK
    ESP_LOGD(TAG, "Enabling I2S external Cirrus Logic CS4344 DAC using GPIO 0 for MCLK");

#define YSW_I2S_BCK_PIN 2
#define YSW_I2S_LRCK_PIN 5
#define YSW_I2S_DATA_PIN 18

    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = 0,
        .dma_buf_count = BUFFER_COUNT,
        .dma_buf_len = SAMPLE_COUNT,
        .use_apll = true,
    };

    $(i2s_driver_install(0, &i2s_config, 0, NULL));

    i2s_pin_config_t pin_config = {
        .bck_io_num = YSW_I2S_BCK_PIN,
        .ws_io_num = YSW_I2S_LRCK_PIN,
        .data_out_num = YSW_I2S_DATA_PIN,
        .data_in_num = -1,
    };

    $(i2s_set_pin(0, &pin_config));

    REG_WRITE(PIN_CTRL, 0b111111110000);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
#endif
}

static void run_synth(void* context)
{
    ysw_i2s_t *i2s = context;

    initialize();

    for (;;) {
        uint8_t buf[BUFFER_SIZE];
        uint32_t bytes_generated = i2s->generate_audio(buf, sizeof(buf));
        size_t write_count = 0;
        $(i2s_write(I2S_NUM_0, buf, bytes_generated, &write_count, portMAX_DELAY));
    }
}

void ysw_i2s_create_task(ysw_i2s_generate_audio_t generate_audio)
{
    ysw_i2s_t *i2s = ysw_heap_allocate(sizeof(ysw_i2s_t));
    i2s->generate_audio = generate_audio;

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.function = run_synth;
    config.context = i2s;

    ysw_task_create(&config);
}
