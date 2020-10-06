// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_synth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "stdint.h"

#define YSW_SYNTH_MOD_MAX_NAME_LENGTH 128

QueueHandle_t ysw_synth_mod_create_task();

typedef enum {
    YSW_SYNTH_MOD_SAMPLE_LOAD = YSW_SYNTH_CUSTOM,
} ysw_synth_mod_message_type_t;

typedef struct {
    char name[YSW_SYNTH_MOD_MAX_NAME_LENGTH];
    uint16_t index;
    uint16_t reppnt;
    uint16_t replen;
    uint8_t volume;
    uint8_t pan;
} ysw_synth_mod_sample_load_t;

typedef struct {
    char name[YSW_SYNTH_MOD_MAX_NAME_LENGTH];
    uint8_t program;
    uint8_t sample;
} ysw_synth_mod_program_patch_t;

typedef struct {
    ysw_synth_mod_message_type_t type;
    union {
        ysw_synth_note_on_t note_on;
        ysw_synth_note_off_t note_off;
        ysw_synth_program_change_t program_change;
        ysw_synth_mod_sample_load_t sample_load;
        ysw_synth_mod_program_patch_t program_patch;
    };
} ysw_synth_mod_message_t;

