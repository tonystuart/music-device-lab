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
#include "esp_log.h"
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "ALSA"

#define ALSA_DEVICE "default"
#define ALSA_CHANNELS 2
#define ALSA_RATE 44100

// See https://www.alsa-project.org/alsa-doc/alsa-lib/index.html
// See https://www.alsa-project.org/alsa-doc/alsa-lib/_2test_2pcm_8c-example.html

void ysw_alsa_initialize(esp_a2d_source_data_cb_t data_cb)
{
    snd_pcm_t *alsa;
    int rc = snd_pcm_open(&alsa, ALSA_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        ESP_LOGE(TAG, "snd_pcm_open failed, device=%s, error=%s", ALSA_DEVICE, snd_strerror(rc));
        abort();
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

    rc = snd_pcm_hw_params(alsa, params);
    if (rc < 0) {
        ESP_LOGE(TAG, "snd_pcm_hw_parms failed, error=%s", snd_strerror(rc));
        abort();
    }

    snd_pcm_uframes_t period_size;
    snd_pcm_hw_params_get_period_size(params, &period_size, 0);

    int buf_size = period_size * ALSA_CHANNELS * 2;
    char *buf = ysw_heap_allocate(buf_size);

    while (data_cb(buf, buf_size) > 0) {
        rc = snd_pcm_writei(alsa, buf, period_size);
        if (rc < 0) {
            ESP_LOGE(TAG, "snd_pcm_writei failed, error=%s", snd_strerror(rc));
            abort();
        }
    }

    snd_pcm_drain(alsa);
    snd_pcm_close(alsa);
    ysw_heap_free(buf);
}

