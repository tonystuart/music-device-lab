// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_main_synth.h"
#include "ysw_music.h"

#if YSW_MAIN_SYNTH_MODEL == 1
#include "ysw_synth_bt.h"
#elif YSW_MAIN_SYNTH_MODEL == 2
#include "ysw_synth_vs.h"
#elif YSW_MAIN_SYNTH_MODEL == 3
#include "ysw_synth_fs.h"
#endif
#include "ysw_synth.h"
#include "ysw_message.h"

#include "esp_log.h"

#define TAG "YSW_MAIN_SYNTH"

static QueueHandle_t synth_queue;

void ysw_main_synth_send(ysw_synth_message_t *message)
{
    if (synth_queue) {
        ysw_message_send(synth_queue, message);
    }
}

void ysw_main_synth_initialize()
{
#if YSW_MAIN_SYNTH_MODEL == 1
    ESP_LOGD(TAG, "ysw_main_synth_initialize: configuring BlueTooth synth");
    synth_queue = ysw_synth_bt_create_task();
#elif YSW_MAIN_SYNTH_MODEL == 2
    ESP_LOGD(TAG, "ysw_main_synth_initialize: configuring VS1053 synth");
    ysw_vs1053_config_t config = {
        .dreq_gpio = -1,
        .xrst_gpio = 0,
        .miso_gpio = 15,
        .mosi_gpio = 17,
        .sck_gpio = 2,
        .xcs_gpio = 16,
        .xdcs_gpio = 4,
        .spi_host = VSPI_HOST,
    };
    synth_queue = ysw_synth_vs_create_task(&config);
#elif YSW_MAIN_SYNTH_MODEL == 3
    ESP_LOGD(TAG, "ysw_main_synth_initialize: configuring FluidSynth");
    synth_queue = ysw_synth_fs_create_task(YSW_MUSIC_SOUNDFONT);
#else
#error Define YSW_MAIN_SYNTH_MODEL
#endif
}

