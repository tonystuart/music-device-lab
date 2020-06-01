// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "seq.h"

#include "synthesizer.h"

#include "ysw_cs.h"
#include "ysw_message.h"
#include "ysw_seq.h"

#include "esp_log.h"

#define TAG "SEQ"

static QueueHandle_t seq_queue;
static seq_cb_t seq_cb;

static void on_note_on(ysw_note_t *note)
{
    if (note->channel == YSW_CS_META_CHANNEL) {
        if (seq_cb) {
            seq_cb_message_t seq_cb_message = {
                .type = META_NOTE,
                .meta_note = note,
            };
            seq_cb(&seq_cb_message);
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

static void on_control_change(ysw_seq_state_t new_state)
{
    if (new_state == YSW_SEQ_IDLE) {
        if (seq_cb) {
            seq_cb_message_t seq_cb_message = {
                .type = NOT_PLAYING,
            };
            seq_cb(&seq_cb_message);
        }
    }
}

void seq_send(ysw_seq_message_t *message)
{
    if (seq_queue) {
        ysw_message_send(seq_queue, message);
    }
}

void seq_initialize(seq_cb_t new_seq_cb)
{
    seq_cb = new_seq_cb;

    ysw_seq_config_t config = {
        .on_note_on = on_note_on,
        .on_note_off = on_note_off,
        .on_program_change = on_program_change,
        .on_control_change = on_control_change,
    };

    seq_queue = ysw_seq_create_task(&config);
}

