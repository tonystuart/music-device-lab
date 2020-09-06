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
#include "ysw_heap.h"
#include "ysw_task.h"
#include "ysw_synth.h"
#include "esp_log.h"
#include "fluidsynth.h"
#include "stdlib.h"

#define TAG "YSW_SYNTH_FS"

typedef struct {
    const char *sf_filename;
    QueueHandle_t input_queue;
    fluid_synth_t *synth;
    fluid_audio_driver_t *driver; // TODO: delete this when done with it
} ysw_fs_t;

static inline void on_note_on(ysw_fs_t *ysw_fs, ysw_synth_note_on_t *m)
{
    ESP_LOGD(TAG, "on_note_on channel=%d, note=%d, velocity=%d", m->channel, m->midi_note, m->velocity);
    fluid_synth_noteon(ysw_fs->synth, m->channel, m->midi_note, m->velocity);
}

static inline void on_note_off(ysw_fs_t *ysw_fs, ysw_synth_note_off_t *m)
{
    ESP_LOGD(TAG, "on_note_off channel=%d, note=%d", m->channel, m->midi_note);
    fluid_synth_noteoff(ysw_fs->synth, m->channel, m->midi_note);
}

static inline void on_program_change(ysw_fs_t *ysw_fs, ysw_synth_program_change_t *m)
{
    fluid_synth_program_change(ysw_fs->synth, m->channel, m->program);
}

static void process_message(ysw_fs_t *ysw_fs, ysw_synth_message_t *message)
{
    switch (message->type) {
        case YSW_SYNTH_NOTE_ON:
            on_note_on(ysw_fs, &message->note_on);
            break;
        case YSW_SYNTH_NOTE_OFF:
            on_note_off(ysw_fs, &message->note_off);
            break;
        case YSW_SYNTH_PROGRAM_CHANGE:
            on_program_change(ysw_fs, &message->program_change);
            break;
        default:
            break;
    }
}

#define INITIAL_GAIN (1)
#define MINIMUM_GAIN (0f)
#define MAXIMUM_GAIN (10f)
#define DEFAULT_GAIN (0.200f)

static void initialize_fluidsynth(ysw_fs_t *ysw_fs)
{
    fluid_settings_t *settings = new_fluid_settings();

    // See fluid_synth_settings
    fluid_settings_setint(settings, "synth.polyphony", 8);
    fluid_settings_setint(settings, "synth.reverb.active", 0);
    fluid_settings_setint(settings, "synth.chorus.active", 0);

    ysw_fs->synth = new_fluid_synth(settings);

    int sfont_id = fluid_synth_sfload(ysw_fs->synth, ysw_fs->sf_filename, 1);
    if (sfont_id == FLUID_FAILED) {
        ESP_LOGE(TAG, "fluid_synth_sfload failed, sf_filename=%s", ysw_fs->sf_filename);
        abort();
    }

    fluid_synth_set_gain(ysw_fs->synth, INITIAL_GAIN);


#ifdef IDF_VER
    fluid_settings_setstr(settings, "audio.driver", "a2dp");
#else
    fluid_settings_setstr(settings, "audio.driver", "alsa");
#endif
    ysw_fs->driver = new_fluid_audio_driver(settings, ysw_fs->synth);
}

static void run_fs_synth(void *parameter)
{
    ysw_fs_t *ysw_fs = parameter;
    // initialize_fluidsynth(ysw_fs);
    for (;;) {
        ysw_synth_message_t message;
        BaseType_t is_message = xQueueReceive(ysw_fs->input_queue, &message, portMAX_DELAY);
        if (is_message) {
            process_message(ysw_fs, &message);
        }
    }
}

QueueHandle_t ysw_synth_fs_create_task(const char *sf_filename)
{
    ysw_fs_t *ysw_fs = ysw_heap_allocate(sizeof(ysw_fs_t));
    ysw_fs->sf_filename = sf_filename;
    ysw_task_config_t config = {
        .name = TAG,
        .function = run_fs_synth,
        .queue = &ysw_fs->input_queue,
        .parameters = ysw_fs,
        .queue_size = YSW_TASK_MEDIUM_QUEUE,
        .item_size = sizeof(ysw_synth_message_t),
        .stack_size = YSW_TASK_MEDIUM_STACK,
        .priority = YSW_TASK_DEFAULT_PRIORITY,
    };
    ESP_LOGI(TAG, "ysw_synth_fs_create_task initializing fluidsynth prior to task creation");
    initialize_fluidsynth(ysw_fs);
    ysw_task_create(&config);
    return ysw_fs->input_queue;
}
