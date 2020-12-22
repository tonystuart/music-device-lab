// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_a2dp_driver.h"
#include "ysw_a2dp_source.h"
#include "ysw_heap.h"

#include "esp_log.h"
#include "math.h"

#define TAG "YSW_A2DP_DRIVER"

#define PI (3.141592654)

// TODO: modify ysw_a2dp_source to pass context to data_cb and pass ysw_a2dp_audio_driver_t

typedef struct
{
    double x;
    uint32_t calls;
    uint32_t iterations;
    fluid_synth_t *synth;
} ysw_a2dp_audio_driver_t;

static ysw_a2dp_audio_driver_t *driver;

// NB: len is in bytes (typically 512 when called from ysw_a2dp_source)
static int32_t data_cb(uint8_t *data, int32_t len)
{
    //ESP_LOGE(TAG, "data_cb entered, len=%d", len);
    if (len < 0 || data == NULL) {
        return 0;
    }

#if 0
    //if (driver->calls++ < 1000) { // about 3 seconds
    if (driver->calls++ < 344) { // about 1 second
        // generate random sequence
        int val = rand() % (1 << 16);
        for (int i = 0; i < (len >> 1); i++) {
            data[(i << 1)] = val & 0xff;
            data[(i << 1) + 1] = (val >> 8) & 0xff;
        }
    } else if (driver->calls < 2000) {
        // 44,100 cycles per second
        // 128 iterations per call (512 / 4)
        // need (44100 / 128) = 344 calls per second to keep the pipeline full.
        // 440 cyles per second
        // need (440 / 344) = 1.27 sine waves per call
        // (3.14 * 2) = 6.28 radians per sine wave
        // need (6.28 * 1.27) = 7.97 'x' cycles per call
        // need (7.97 / 128) = 0.0622 'x' increment per iteration
        //ESP_LOGD(TAG, "len=%d", len);
        int16_t *p = (int16_t*)data;
        for (int i = 0; i < len; i+= 4) {
            *p++ = 32767 * sin(driver->x);
            if (driver->x > (2 * PI)) {
                driver->x = 0;
            } else {
                driver->x += 0.0622;
            }
            //ESP_LOGD(TAG, "iterations=%d, x=%g", driver->iterations++, driver->x);
        }
    }
    else
#endif
    {
        //ESP_LOGD(TAG, "len=%d", len);
        fluid_synth_write_s16(driver->synth, len / 4, (int16_t *)data, 0, 2, (int16_t *)data, 1, 2);

    }
    return len;
}

fluid_audio_driver_t *ysw_a2dp_driver_new(fluid_settings_t *settings, fluid_synth_t *synth)
{
    driver = ysw_heap_allocate(sizeof(ysw_a2dp_audio_driver_t));

    driver->synth = synth;

    int period_size;
    fluid_settings_getint(settings, "audio.period-size", &period_size);

    double sample_rate;
    fluid_settings_getnum(settings, "synth.sample-rate", &sample_rate);

    ESP_LOGD(TAG, "new_a2dp_audio_driver period_size=%d, sample_rate=%g", period_size, sample_rate);

    ysw_a2dp_source_initialize(data_cb);

    return (fluid_audio_driver_t *)driver;
}

void ysw_a2dp_driver_delete(fluid_audio_driver_t *p)
{
    ysw_a2dp_audio_driver_t *driver = (ysw_a2dp_audio_driver_t *) p;
    ysw_heap_free(driver);
}

