// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "sequencer.h"

#include "synthesizer.h"

#include "ysw_cs.h"
#include "ysw_message.h"
#include "ysw_sequencer.h"

#include "esp_log.h"

#define TAG "SEQUENCER"

static QueueHandle_t sequencer_queue;
static sequencer_cb_t sequencer_cb;

static void on_note_on(note_t *note)
{
    if (note->channel == YSW_CS_META_CHANNEL) {
        if (sequencer_cb) {
            sequencer_cb_message_t sequencer_cb_message = {
                .type = META_NOTE,
                .meta_note = note,
            };
            sequencer_cb(&sequencer_cb_message);
        }
    } else {
        ysw_synthesizer_message_t message = {
            .type = YSW_SYNTHESIZER_NOTE_ON,
            .note_on.channel = note->channel,
            .note_on.midi_note = note->midi_note,
            .note_on.velocity = note->velocity,
        };
        synthesizer_send(&message);
    }
}

static void on_note_off(uint8_t channel, uint8_t midi_note)
{
    ysw_synthesizer_message_t message = {
        .type = YSW_SYNTHESIZER_NOTE_OFF,
        .note_off.channel = channel,
        .note_off.midi_note = midi_note,
    };
    synthesizer_send(&message);
}

static void on_program_change(uint8_t channel, uint8_t program)
{
    ysw_synthesizer_message_t message = {
        .type = YSW_SYNTHESIZER_PROGRAM_CHANGE,
        .program_change.channel = channel,
        .program_change.program = program,
    };
    synthesizer_send(&message);
}

static void on_state_change(ysw_sequencer_state_t new_state)
{
    if (new_state == YSW_SEQUENCER_STATE_NOT_PLAYING) {
        if (sequencer_cb) {
            sequencer_cb_message_t sequencer_cb_message = {
                .type = NOT_PLAYING,
            };
            sequencer_cb(&sequencer_cb_message);
        }
    }
}

void sequencer_send(ysw_sequencer_message_t *message)
{
    if (sequencer_queue) {
        ysw_message_send(sequencer_queue, message);
    }
}

void sequencer_initialize(sequencer_cb_t new_sequencer_cb)
{
    sequencer_cb = new_sequencer_cb;

    ysw_sequencer_config_t config = {
        .on_note_on = on_note_on,
        .on_note_off = on_note_off,
        .on_program_change = on_program_change,
        .on_state_change = on_state_change,
    };

    sequencer_queue = ysw_sequencer_create_task(&config);
}

