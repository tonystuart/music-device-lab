// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_main_synthesizer.h"

#if YSW_MAIN_SYNTHESIZER_MODEL == 1
#include "ysw_ble_synthesizer.h"
#elif YSW_MAIN_SYNTHESIZER_MODEL == 2
#include "ysw_vs1053_synthesizer.h"
#endif
#include "ysw_synthesizer.h"
#include "ysw_message.h"

#include "esp_log.h"

#define TAG "YSW_MAIN_SYNTHESIZER"

static QueueHandle_t synthesizer_queue;

void ysw_main_synthesizer_send(ysw_synthesizer_message_t *message)
{
    if (synthesizer_queue) {
        ysw_message_send(synthesizer_queue, message);
    }
}

void ysw_main_synthesizer_initialize()
{
#if YSW_MAIN_SYNTHESIZER_MODEL == 1
    ESP_LOGD(TAG, "ysw_main_synthesizer_initialize: configuring BLE synthesizer");
    synthesizer_queue = ysw_ble_synthesizer_create_task();
#elif YSW_MAIN_SYNTHESIZER_MODEL == 2
    ESP_LOGD(TAG, "ysw_main_synthesizer_initialize: configuring VS1053 synthesizer");
    ysw_vs1053_synthesizer_config_t config = {
        .dreq_gpio = -1,
        .xrst_gpio = 0,
        .miso_gpio = 15,
        .mosi_gpio = 17,
        .sck_gpio = 2,
        .xcs_gpio = 16,
        .xdcs_gpio = 4,
        .spi_host = VSPI_HOST,
    };
    synthesizer_queue = ysw_vs1053_synthesizer_create_task(&config);
#else
#error Define YSW_MAIN_SYNTHESIZER_MODEL
#endif
}

