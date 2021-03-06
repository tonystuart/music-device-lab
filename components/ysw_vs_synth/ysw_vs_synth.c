// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_vs_synth.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_task.h"
#include "ysw_vs1053.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#define TAG "YSW_VS_SYNTH"

typedef struct {
    ysw_vs1053_config_t config;
} ysw_vs_synth_t;

static inline void on_note_on(ysw_vs_synth_t *vs_synth, ysw_event_note_on_t *m)
{
    //ESP_LOGD(TAG, "on_note_on channel=%d, note=%d, velocity=%d", m->channel, m->midi_note, m->velocity);
    ysw_vs1053_set_note_on(m->channel, m->midi_note, m->velocity);
}

static inline void on_note_off(ysw_vs_synth_t *vs_synth, ysw_event_note_off_t *m)
{
    //ESP_LOGD(TAG, "on_note_off channel=%d, note=%d", m->channel, m->midi_note);
    ysw_vs1053_set_note_off(m->channel, m->midi_note);
}

static inline void on_program_change(ysw_vs_synth_t *vs_synth, ysw_event_program_change_t *m)
{
    ysw_vs1053_select_program(m->channel, m->program);
}

static void process_event(void *context, ysw_event_t *event)
{
    ysw_vs_synth_t *vs_synth = context;
    switch (event->header.type) {
        case YSW_EVENT_NOTE_ON:
            on_note_on(vs_synth, &event->note_on);
            break;
        case YSW_EVENT_NOTE_OFF:
            on_note_off(vs_synth, &event->note_off);
            break;
        case YSW_EVENT_PROGRAM_CHANGE:
            on_program_change(vs_synth, &event->program_change);
            break;
        default:
            break;
    }
}

void ysw_vs_synth_create_task(ysw_bus_t *bus, ysw_vs1053_config_t *vs1053_config)
{
    assert(portTICK_PERIOD_MS == 1);
    ysw_vs_synth_t *vs_synth = ysw_heap_allocate(sizeof(ysw_vs_synth_t));
    vs_synth->config = *vs1053_config;
    ysw_vs1053_initialize(&vs_synth->config);

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.event_handler = process_event;
    config.context = vs_synth;

    ysw_task_t *task = ysw_task_create(&config);

    ysw_task_subscribe(task, YSW_ORIGIN_EDITOR);
    ysw_task_subscribe(task, YSW_ORIGIN_SEQUENCER);
}
