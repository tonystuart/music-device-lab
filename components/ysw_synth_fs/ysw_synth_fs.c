// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_synth_fs.h"
#include "ysw_common.h"
#include "ysw_task.h"
#include "ysw_synth.h"
#include "esp_log.h"
#include "fluidsynth.h"

#define TAG "YSW_SYNTH_FS"

static QueueHandle_t input_queue;

static inline void on_note_on(ysw_synth_note_on_t *m)
{
    //ESP_LOGD(TAG, "on_note_on channel=%d, note=%d, velocity=%d", m->channel, m->midi_note, m->velocity);
}

static inline void on_note_off(ysw_synth_note_off_t *m)
{
    //ESP_LOGD(TAG, "on_note_off channel=%d, note=%d", m->channel, m->midi_note);
}

static inline void on_program_change(ysw_synth_program_change_t *m)
{
}

static void process_message(ysw_synth_message_t *message)
{
    switch (message->type) {
        case YSW_SYNTH_NOTE_ON:
            on_note_on(&message->note_on);
            break;
        case YSW_SYNTH_NOTE_OFF:
            on_note_off(&message->note_off);
            break;
        case YSW_SYNTH_PROGRAM_CHANGE:
            on_program_change(&message->program_change);
            break;
        default:
            break;
    }
}

static fluid_settings_t *settings;
static fluid_synth_t *synth;
static fluid_audio_driver_t *adriver;
static int sfont_id;

static void run_fs_synth(void* parameters)
{
    settings = new_fluid_settings();
    synth = new_fluid_synth(settings);
    adriver = new_fluid_audio_driver(settings, synth);
    sfont_id = fluid_synth_sfload(synth, "", 1);
    if (sfont_id == FLUID_FAILED) {
        ESP_LOGE(TAG, "fluid_synth_sfload failed, sf_filename=%s", SF_FILENAME);
        abort();
    }

    for (;;) {
        ysw_synth_message_t message;
        BaseType_t is_message = xQueueReceive(input_queue, &message, portMAX_DELAY);
        if (is_message) {
            process_message(&message);
        }
    }
}

QueueHandle_t ysw_synth_fs_create_task()
{
    ysw_task_create_standard(TAG, run_fs_synth, &input_queue, sizeof(ysw_synth_message_t));
    return input_queue;
}
