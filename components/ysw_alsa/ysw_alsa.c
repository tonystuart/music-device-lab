// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_alsa.h"
#include "ysw_heap.h"
#include "ysw_system.h"
#include "esp_log.h"
#include <alsa/asoundlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "YSW_ALSA"

#define ALSA_DEVICE "default"
#define ALSA_CHANNELS 2
#define ALSA_RATE 44100

// See https://www.alsa-project.org/alsa-doc/alsa-lib/index.html
// See https://www.alsa-project.org/alsa-doc/alsa-lib/_2test_2pcm_8c-example.html

void ysw_alsa_initialize(esp_a2d_source_data_cb_t data_cb)
{
    snd_pcm_t *alsa;
    // int rc = snd_pcm_open(&alsa, ALSA_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
    int rc = snd_pcm_open(&alsa, "hw:0,0,0", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        ESP_LOGE(TAG, "snd_pcm_open failed, device=%s, error=%s", ALSA_DEVICE, snd_strerror(rc));
        if (rc == -EBUSY) {
            ESP_LOGE(TAG, "use 'pacmd suspend true' to suspend pulseaudio so that you can open the hw device");
        }
        ESP_LOGW(TAG, "falling back to device=%s, latency may be significant", ALSA_DEVICE);
        int rc = snd_pcm_open(&alsa, ALSA_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
        if (rc < 0) {
            ESP_LOGE(TAG, "fallback failed, terminating");
            abort();
        }
    }

    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(alsa, params);

    rc = snd_pcm_hw_params_set_access(alsa, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (rc < 0) {
        ESP_LOGE(TAG, "snd_pcm_hw_params_set_access failed, error=%s", snd_strerror(rc));
        abort();
    }

    rc = snd_pcm_hw_params_set_format(alsa, params, SND_PCM_FORMAT_S16_LE);
    if (rc < 0) {
        ESP_LOGE(TAG, "snd_pcm_hw_params_set_format failed, error=%s", snd_strerror(rc));
        abort();
    }

    rc = snd_pcm_hw_params_set_channels(alsa, params, ALSA_CHANNELS);
    if (rc < 0) {
        ESP_LOGE(TAG, "snd_pcm_hw_params_set_channels failed, error=%s", snd_strerror(rc));
        abort();
    }

    rc = snd_pcm_hw_params_set_rate(alsa, params, ALSA_RATE, 0);
    if (rc < 0) {
        ESP_LOGE(TAG, "snd_pcm_hw_params_set_rate_near failed, error=%s", snd_strerror(rc));
        abort();
    }

    rc = snd_pcm_hw_params_set_period_size(alsa, params, 128, 0);
    if (rc < 0) {
        ESP_LOGE(TAG, "snd_pcm_hw_params_set_rate_near failed, error=%s", snd_strerror(rc));
        abort();
    }

    rc = snd_pcm_hw_params(alsa, params);
    if (rc < 0) {
        ESP_LOGE(TAG, "snd_pcm_hw_parms failed, error=%s", snd_strerror(rc));
        abort();
    }

    snd_pcm_uframes_t period_size;
    snd_pcm_hw_params_get_period_size(params, &period_size, 0);

    int buf_size = period_size * ALSA_CHANNELS * 2;
    uint8_t *buf = ysw_heap_allocate(buf_size);

#ifdef DISPLAY_LATENCY
    uint64_t samples_generated = 0;
    int start_time = ysw_system_get_reference_time_in_millis();
#endif

    while (data_cb(buf, buf_size) > 0) {
        rc = snd_pcm_writei(alsa, buf, period_size);
        if (rc < 0) {
            ESP_LOGE(TAG, "snd_pcm_writei failed, error=%s", snd_strerror(rc));
            snd_pcm_prepare(alsa);
        }
#ifdef DISPLAY_LATENCY
        samples_generated += period_size;
        int current_time = ysw_system_get_reference_time_in_millis();
        if (((current_time - start_time) / 1000) > 10) {
            int millis_generated = (samples_generated * 1000) / 44100;
            int elapsed_millis = current_time - start_time;
            int latency = millis_generated - elapsed_millis;
            ESP_LOGI(TAG, "samples_generated=%d, millis_generated=%ld, elapsed_millis=%d, latency=%d", samples_generated, millis_generated, elapsed_millis, latency);
            samples_generated = 0;
            start_time = current_time;
        }
#endif
    }

    snd_pcm_drain(alsa);
    snd_pcm_close(alsa);
    ysw_heap_free(buf);
}

