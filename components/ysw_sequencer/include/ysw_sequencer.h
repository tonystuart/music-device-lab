// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "ysw_song.h"
#include "ysw_task.h"

#define PLAYBACK_SPEED_PERCENT 100

typedef void (*ysw_sequencer_on_note_on_t)(uint8_t channel, uint8_t midi_note, uint8_t velocity);
typedef void (*ysw_sequencer_on_note_off_t)(uint8_t channel, uint8_t midi_note);
typedef void (*ysw_sequencer_on_program_change_t)(uint8_t channel, uint8_t program);

typedef struct {
    ysw_sequencer_on_note_on_t on_note_on;
    ysw_sequencer_on_note_off_t on_note_off;
    ysw_sequencer_on_program_change_t on_program_change;
} ysw_sequencer_config_t;

typedef enum {
    YSW_SEQUENCER_INITIALIZE,
    YSW_SEQUENCER_SET_TEMPO,
    YSW_SEQUENCER_PLAY,
    YSW_SEQUENCER_PAUSE,
} ysw_sequencer_message_type_t;

typedef struct {
    note_t *notes; // must remain accessible for duration of playback
    uint32_t note_count;
    bool is_loop;
} ysw_sequencer_initialize_t;

typedef struct {
    uint8_t percent;
} ysw_sequencer_set_tempo_t;

typedef struct {
    ysw_sequencer_message_type_t type;
    union {
        ysw_sequencer_initialize_t initialize_message;
        ysw_sequencer_set_tempo_t set_tempo_message;
    };
} ysw_sequencer_message_t;

QueueHandle_t ysw_sequencer_create_task(ysw_sequencer_config_t *config);
