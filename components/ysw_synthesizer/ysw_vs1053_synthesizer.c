// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_vs1053_synthesizer.h"

#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "ysw_common.h"
#include "ysw_vs1053_driver.h"
#include "ysw_task.h"
#include "ysw_synthesizer.h"

#define TAG "VS1053"

static ysw_vs1053_synthesizer_config_t config;

static QueueHandle_t input_queue;

static inline void on_note_on(ysw_synthesizer_note_on_t *m)
{
    //ESP_LOGD(TAG, "on_note_on channel=%d, note=%d, velocity=%d", m->channel, m->midi_note, m->velocity);
    ysw_vs1053_synthesizer_set_note_on(m->channel, m->midi_note, m->velocity);
}

static inline void on_note_off(ysw_synthesizer_note_off_t *m)
{
    //ESP_LOGD(TAG, "on_note_off channel=%d, note=%d", m->channel, m->midi_note);
    ysw_vs1053_synthesizer_set_note_off(m->channel, m->midi_note);
}

static inline void on_program_change(ysw_synthesizer_program_change_t *m)
{
    ysw_vs1053_synthesizer_select_program(m->channel, m->program);
}

static void process_message(ysw_synthesizer_message_t *message)
{
    switch (message->type) {
        case YSW_SYNTHESIZER_NOTE_ON:
            on_note_on(&message->note_on);
            break;
        case YSW_SYNTHESIZER_NOTE_OFF:
            on_note_off(&message->note_off);
            break;
        case YSW_SYNTHESIZER_PROGRAM_CHANGE:
            on_program_change(&message->program_change);
            break;
        default:
            break;
    }
}

static void run_vs_synth(void* parameters)
{
    ESP_LOGD(TAG, "run_vs_synth core=%d", xPortGetCoreID());
    if (portTICK_PERIOD_MS != 1) {
        ESP_LOGE(TAG, "FreeRTOS tick rate is not 1000 Hz");
        abort();
    }
    ysw_vs1053_synthesizer_initialize(&config);
    for (;;) {
        ysw_synthesizer_message_t message;
        BaseType_t is_message = xQueueReceive(input_queue, &message, portMAX_DELAY);
        if (is_message) {
            process_message(&message);
        }
    }
}

QueueHandle_t ysw_vs1053_synthesizer_create_task(ysw_vs1053_synthesizer_config_t *vs1053_config)
{
    config = *vs1053_config;
    ysw_task_create_standard(TAG, run_vs_synth, &input_queue, sizeof(ysw_synthesizer_message_t));
    return input_queue;
}
