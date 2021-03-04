// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_bus.h"
#include "ysw_midi.h"
#include "hash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "stdint.h"

#define MAX_VOICES 32

#define YSW_MOD_MAX_BANKS 2

typedef enum {
    YSW_MOD_IDLE = 0,
    YSW_MOD_NOTE_ON,
    YSW_MOD_DELAY,
    YSW_MOD_ATTACK,
    YSW_MOD_HOLD,
    YSW_MOD_DECAY,
    YSW_MOD_SUSTAIN,
    YSW_MOD_NOTE_OFF,
    YSW_MOD_RELEASE,
} ysw_mod_state_t;

typedef enum {
    YSW_MOD_PAN_LEFT,
    YSW_MOD_PAN_CENTER,
    YSW_MOD_PAN_RIGHT,
} ysw_mod_pan_t;

typedef enum {
    YSW_MOD_LOOP_NONE,
    YSW_MOD_LOOP_CONTINUOUS,
    YSW_MOD_LOOP_UNUSED,
    YSW_MOD_LOOP_THROUGH,
} ysw_mod_loop_t;

typedef struct {
    char *name;
    int8_t *data;
    uint16_t length;
    uint32_t loop_start;
    uint32_t loop_end;
    ysw_mod_loop_t loop_type;
    uint8_t volume;
    ysw_mod_pan_t pan;
    int16_t attenuation;
    int16_t fine_tune;
    int16_t root_key;
    uint32_t delay;
    uint32_t attack;
    uint32_t hold;
    uint32_t decay;
    uint32_t release;
    uint32_t sustain;
    uint8_t from_note;
    uint8_t to_note;
    uint8_t from_velocity;
    uint8_t to_velocity;
} ysw_mod_sample_t;

typedef struct {
    ysw_mod_sample_t *sample;
    uint32_t samppos; // scaled position in sample
    uint32_t sampinc;
    uint32_t time;
    uint32_t length;
    uint32_t loop_start;
    uint32_t loop_end;
    ysw_mod_loop_t loop_type;
    uint32_t iterations;
    uint32_t next_change;
    int32_t amplitude;
    int32_t ramp_step;
    uint8_t volume;
    uint8_t channel;
    uint8_t midi_note;
    ysw_mod_state_t state;
} voice_t;

typedef struct {
    ysw_array_t *samples;
} ysw_mod_instrument_t;

typedef struct {
    char *name;
    uint8_t bank;
    uint8_t num;
    ysw_array_t *instruments;
} ysw_mod_preset_t;

typedef struct {
    uint8_t num;
    ysw_mod_preset_t *presets[YSW_MIDI_MAX_COUNT];
} ysw_mod_bank_t;

typedef struct {
    uint8_t active_notes[YSW_MIDI_MAX_CHANNELS][YSW_MIDI_MAX_COUNT];
    uint8_t channel_programs[YSW_MIDI_MAX_CHANNELS];
    uint32_t playrate;
    uint32_t voice_time;
    voice_t voices[MAX_VOICES];
    uint16_t voice_count;
    uint16_t mod_loaded;
    int16_t last_left_sample;
    int16_t last_right_sample;;
    int16_t stereo;
    int16_t stereo_separation;
    int16_t filter;
    uint16_t percent_gain;
    SemaphoreHandle_t mutex;
    const char *folder;
    ysw_mod_bank_t banks[YSW_MOD_MAX_BANKS];
    hash_t *sample_map;
} ysw_mod_synth_t;

typedef enum {
    YSW_MOD_16BIT_SIGNED,
    YSW_MOD_16BIT_UNSIGNED,
} ysw_mod_sample_type_t;

ysw_mod_synth_t *ysw_mod_synth_create_task(ysw_bus_t *bus);

void ysw_mod_generate_samples(ysw_mod_synth_t *ysw_mod_synth,
        int16_t *buffer, uint32_t number, ysw_mod_sample_type_t sample_type);
