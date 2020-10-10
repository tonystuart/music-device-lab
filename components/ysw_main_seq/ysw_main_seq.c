// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_main_seq.h"

#include "ysw_message.h"
#include "ysw_seq.h"
#include "ysw_main_synth.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "stdlib.h"

#define TAG "YSW_MAIN_SEQ"

static bool is_loop;
static QueueHandle_t seq_queue;

static void on_note_on(ysw_note_t *note)
{
    ysw_synth_message_t message = {
        .type = YSW_SYNTH_NOTE_ON,
        .note_on.channel = note->channel,
        .note_on.midi_note = note->midi_note,
        .note_on.velocity = note->velocity,
    };
    ysw_main_synth_send(&message);
}

static void on_note_off(uint8_t channel, uint8_t midi_note)
{
    ysw_synth_message_t message = {
        .type = YSW_SYNTH_NOTE_OFF,
        .note_off.channel = channel,
        .note_off.midi_note = midi_note,
    };
    ysw_main_synth_send(&message);
}

static void on_program_change(uint8_t channel, uint8_t program)
{
    ysw_synth_message_t message = {
        .type = YSW_SYNTH_PROGRAM_CHANGE,
        .program_change.channel = channel,
        .program_change.program = program,
    };
    ysw_main_synth_send(&message);
}

void ysw_main_seq_send(ysw_seq_message_t *message)
{
    if (message->type == YSW_SEQ_LOOP) {
        is_loop = message->loop.loop;
        ESP_LOGD(TAG, "ysw_main_seq_send loop=%d", is_loop);
    }
    if (seq_queue) {
        ysw_message_send(seq_queue, message);
    }
}

void ysw_main_seq_rendezvous(ysw_seq_message_t *message)
{
    if (seq_queue) {
        message->rendezvous = xEventGroupCreate();
        if (!message->rendezvous) {
            ESP_LOGE(TAG, "xEventGroupCreate failed");
            abort();
        }
        ESP_LOGD(TAG, "ysw_main_seq_rendezvous sending message");
        ysw_message_send(seq_queue, message);
        ESP_LOGD(TAG, "ysw_main_seq_rendezvous waiting for notification");
        xEventGroupWaitBits(message->rendezvous, 1, false, true, portMAX_DELAY);
        ESP_LOGD(TAG, "ysw_main_seq_rendezvous deleting event group");
        vEventGroupDelete(message->rendezvous);
    }
}

void ysw_main_seq_initialize()
{
    ysw_seq_config_t config = {
        .on_note_on = on_note_on,
        .on_note_off = on_note_off,
        .on_program_change = on_program_change,
    };

    seq_queue = ysw_seq_create_task(&config);
}

void ysw_main_seq_init_loop_btn(lv_obj_t *btn)
{
    ESP_LOGD(TAG, "ysw_main_seq_init_loop_btn loop=%d", is_loop);
    lv_btn_set_checkable(btn, true);
    lv_btn_set_state(btn, is_loop ? LV_BTN_STATE_CHECKED_RELEASED : LV_BTN_STATE_RELEASED);
}

void ysw_main_seq_on_stop(void *context, lv_obj_t *btn)
{
    ysw_seq_message_t message = {
            .type = YSW_SEQ_STOP,
    };

    ysw_main_seq_send(&message);
}

void ysw_main_seq_on_loop(void *context, lv_obj_t *btn)
{
    ysw_seq_message_t message = {
            .type = YSW_SEQ_LOOP,
            //.loop.loop = lv_btn_get_state(btn) == LV_BTN_STATE_RELEASED,
            .loop.loop = lv_btn_get_state(btn) == LV_BTN_STATE_CHECKED_RELEASED,
    };
    ysw_main_seq_send(&message);
}

