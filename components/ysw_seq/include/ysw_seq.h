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
#include "ysw_note.h"

#define YSW_SEQ_SPEED_DEFAULT 100

typedef enum {
    YSW_SEQ_LOOP_DONE,
    YSW_SEQ_PLAY_DONE,
    YSW_SEQ_IDLE,
} ysw_seq_state_t;

typedef void (*ysw_seq_note_on_cb_t)(ysw_note_t *note);
typedef void (*ysw_seq_note_off_cb_t)(uint8_t channel, uint8_t midi_note);
typedef void (*ysw_seq_program_change_cb_t)(uint8_t channel, uint8_t program);
typedef void (*ysw_seq_control_cb_t)(ysw_seq_state_t state);

typedef struct {
    ysw_seq_note_on_cb_t on_note_on;
    ysw_seq_note_off_cb_t on_note_off;
    ysw_seq_program_change_cb_t on_program_change;
    ysw_seq_control_cb_t on_state_change;
} ysw_seq_config_t;

typedef enum {
    YSW_SEQ_PLAY,
    YSW_SEQ_PAUSE,
    YSW_SEQ_RESUME,
    YSW_SEQ_TEMPO,
    YSW_SEQ_LOOP,
    YSW_SEQ_STAGE,
    YSW_SEQ_SPEED,
} ysw_seq_message_type_t;

typedef struct {
    ysw_note_t *notes; // must remain accessible for duration of playback
    uint32_t note_count;
    uint8_t tempo;
} ysw_seq_clip_t;

typedef struct {
    uint8_t qnpm;
} ysw_seq_tempo_t;

typedef struct {
    bool loop;
} ysw_seq_loop_t;

typedef struct {
    uint8_t percent;
} ysw_seq_speed_t;

typedef struct {
    ysw_seq_message_type_t type;
    union {
        ysw_seq_clip_t play;
        ysw_seq_tempo_t tempo;
        ysw_seq_loop_t loop;
        ysw_seq_clip_t stage;
        ysw_seq_speed_t speed;
    };
} ysw_seq_message_t;

QueueHandle_t ysw_seq_create_task(ysw_seq_config_t *config);
