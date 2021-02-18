// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_i2s_driver.h"
#include "ysw_i2s.h"
#include "ysw_heap.h"

#include "esp_log.h"

#define TAG "YSW_I2S_DRIVER"

typedef struct
{
    double x;
    uint32_t calls;
    uint32_t iterations;
    fluid_synth_t *synth;
} ysw_i2s_audio_driver_t;

static ysw_i2s_audio_driver_t *driver;

static int32_t generate_audio(uint8_t *buffer, int32_t bytes_requested)
{
    //ESP_LOGD(TAG, "generate_audio driver=%p, driver->synth=%p, buffer=%p, bytes_requested=%d", driver, driver->synth, buffer, bytes_requested);

    int32_t bytes_generated = 0;
    if (buffer && bytes_requested) {
        fluid_synth_write_s16(driver->synth, bytes_requested / 4,
                (int16_t *)buffer, 0, 2, (int16_t *)buffer, 1, 2);
        bytes_generated = bytes_requested;
    }
    return bytes_generated;
}

static void nop(void)
{
}

fluid_audio_driver_t *ysw_i2s_driver_new(fluid_settings_t *settings, fluid_synth_t *synth)
{
    ESP_LOGD(TAG, "ysw_i2s_driver_new synth=%p", synth);
    driver = ysw_heap_allocate(sizeof(ysw_i2s_audio_driver_t));
    driver->synth = synth;
    ESP_LOGD(TAG, "ysw_i2s_driver_new driver=%p, driver->synth=%p", driver, driver->synth);

    int period_size;
    fluid_settings_getint(settings, "audio.period-size", &period_size);

    double sample_rate;
    fluid_settings_getnum(settings, "synth.sample-rate", &sample_rate);

    ESP_LOGD(TAG, "new_i2s_audio_driver period_size=%d, sample_rate=%g", period_size, sample_rate);

    ysw_i2s_create_task(nop, generate_audio);

    return (fluid_audio_driver_t *)driver;
}

void ysw_i2s_driver_delete(fluid_audio_driver_t *p)
{
    ysw_i2s_audio_driver_t *driver = (ysw_i2s_audio_driver_t *) p;
    ysw_heap_free(driver);
}

