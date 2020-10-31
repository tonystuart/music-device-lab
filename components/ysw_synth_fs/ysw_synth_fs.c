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
#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_task.h"
#include "esp_log.h"
#include "fluidsynth.h"
#include "stdlib.h"

#define TAG "YSW_SYNTH_FS"

typedef struct {
    const char *sf_filename;
    QueueHandle_t queue;
    fluid_synth_t *synth;
    fluid_audio_driver_t *driver; // TODO: delete this when done with it
} context_t;

static inline void on_note_on(context_t *context, ysw_event_note_on_t *m)
{
    ESP_LOGD(TAG, "on_note_on channel=%d, note=%d, velocity=%d", m->channel, m->midi_note, m->velocity);
    fluid_synth_noteon(context->synth, m->channel, m->midi_note, m->velocity);
}

static inline void on_note_off(context_t *context, ysw_event_note_off_t *m)
{
    ESP_LOGD(TAG, "on_note_off channel=%d, note=%d", m->channel, m->midi_note);
    fluid_synth_noteoff(context->synth, m->channel, m->midi_note);
}

static inline void on_program_change(context_t *context, ysw_event_program_change_t *m)
{
    fluid_synth_program_change(context->synth, m->channel, m->program);
}

static void process_event(void *caller_context, ysw_event_t *event)
{
    context_t *context = caller_context;
    switch (event->header.type) {
        case YSW_EVENT_NOTE_ON:
            on_note_on(context, &event->note_on);
            break;
        case YSW_EVENT_NOTE_OFF:
            on_note_off(context, &event->note_off);
            break;
        case YSW_EVENT_PROGRAM_CHANGE:
            on_program_change(context, &event->program_change);
            break;
        default:
            break;
    }
}

#define INITIAL_GAIN (1)
#define MINIMUM_GAIN (0f)
#define MAXIMUM_GAIN (10f)
#define DEFAULT_GAIN (0.200f)

static void initialize_synthesizer(context_t *context)
{
    fluid_settings_t *settings = new_fluid_settings();

    // See fluid_synth_settings
    fluid_settings_setint(settings, "synth.polyphony", 8);
    fluid_settings_setint(settings, "synth.reverb.active", 0);
    fluid_settings_setint(settings, "synth.chorus.active", 0);

    context->synth = new_fluid_synth(settings);

    int sfont_id = fluid_synth_sfload(context->synth, context->sf_filename, 1);
    if (sfont_id == FLUID_FAILED) {
        ESP_LOGE(TAG, "fluid_synth_sfload failed, sf_filename=%s", context->sf_filename);
        abort();
    }

    fluid_synth_set_gain(context->synth, INITIAL_GAIN);

#ifdef IDF_VER
    fluid_settings_setstr(settings, "audio.driver", "a2dp");
#else
    fluid_settings_setstr(settings, "audio.driver", "alsa");
#endif
    context->driver = new_fluid_audio_driver(settings, context->synth);
}

void ysw_synth_fs_create_task(ysw_bus_h bus, const char *sf_filename)
{
    context_t *context = ysw_heap_allocate(sizeof(context_t));
    context->sf_filename = sf_filename;
    initialize_synthesizer(context);

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.event_handler = process_event;
    config.caller_context = context;

    ysw_task_h task = ysw_task_create(&config);

    ysw_task_subscribe(task, YSW_ORIGIN_EDITOR);
    ysw_task_subscribe(task, YSW_ORIGIN_SEQUENCER);
}
