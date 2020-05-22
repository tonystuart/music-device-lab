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
#include "ysw_song.h"

#define YSW_SEQUENCER_SPEED_DEFAULT 100

typedef enum {
    YSW_SEQUENCER_STATE_LOOP_COMPLETE,
    YSW_SEQUENCER_STATE_PLAYBACK_COMPLETE,
} ysw_sequencer_state_t;

typedef void (*ysw_sequencer_on_note_on_t)(note_t *note);
typedef void (*ysw_sequencer_on_note_off_t)(uint8_t channel, uint8_t midi_note);
typedef void (*ysw_sequencer_on_program_change_t)(uint8_t channel, uint8_t program);
typedef void (*ysw_sequencer_on_state_change_t)(ysw_sequencer_state_t state);

typedef struct {
    ysw_sequencer_on_note_on_t on_note_on;
    ysw_sequencer_on_note_off_t on_note_off;
    ysw_sequencer_on_program_change_t on_program_change;
    ysw_sequencer_on_state_change_t on_state_change;
} ysw_sequencer_config_t;

typedef enum {
    YSW_SEQUENCER_PLAY,
    YSW_SEQUENCER_PAUSE,
    YSW_SEQUENCER_RESUME,
    YSW_SEQUENCER_TEMPO,
    YSW_SEQUENCER_LOOP,
    YSW_SEQUENCER_STAGE,
    YSW_SEQUENCER_SPEED,
} ysw_sequencer_message_type_t;

typedef struct {
    note_t *notes; // must remain accessible for duration of playback
    uint32_t note_count;
    uint8_t tempo;
} ysw_sequencer_clip_t;

typedef struct {
    uint8_t qnpm;
} ysw_sequencer_tempo_t;

typedef struct {
    bool loop;
} ysw_sequencer_loop_t;

typedef struct {
    uint8_t percent;
} ysw_sequencer_speed_t;

typedef struct {
    ysw_sequencer_message_type_t type;
    union {
        ysw_sequencer_clip_t play;
        ysw_sequencer_tempo_t tempo;
        ysw_sequencer_loop_t loop;
        ysw_sequencer_clip_t stage;
        ysw_sequencer_speed_t speed;
    };
} ysw_sequencer_message_t;

QueueHandle_t ysw_sequencer_create_task(ysw_sequencer_config_t *config);
