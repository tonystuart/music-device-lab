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
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "stdint.h"

#define MAX_VOICES 32

typedef enum {
    YSW_MOD_PAN_LEFT,
    YSW_MOD_PAN_CENTER,
    YSW_MOD_PAN_RIGHT,
} ysw_mod_pan_t;

typedef struct {
    int8_t *data;
    uint16_t length;
    uint16_t reppnt;
    uint16_t replen;
    uint8_t volume;
    ysw_mod_pan_t pan;
} ysw_mod_sample_t;

typedef ysw_mod_sample_t *(*ysw_mod_sample_provider_t)(void *context, uint8_t program, uint8_t note);

typedef struct {
    void* context;
    ysw_mod_sample_provider_t provide_sample;
} ysw_mod_host_t;

typedef struct {
    ysw_mod_sample_t *sample;
    uint32_t samppos;
    uint32_t sampinc;
    uint32_t time;
    uint16_t length;
    uint16_t reppnt;
    uint16_t replen;
    uint16_t period;
    uint8_t volume;
    uint8_t channel;
    uint8_t  midi_note;
} voice_t;

typedef struct {
    uint8_t active_notes[YSW_MIDI_MAX_CHANNELS][YSW_MIDI_MAX_COUNT];
    uint8_t channel_programs[YSW_MIDI_MAX_CHANNELS];
    uint32_t playrate;
    uint32_t sampleticksconst;
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
    ysw_mod_host_t *mod_host;
} ysw_mod_synth_t;

typedef enum {
    YSW_MOD_16BIT_SIGNED,
    YSW_MOD_16BIT_UNSIGNED,
} ysw_mod_sample_type_t;

ysw_mod_synth_t *ysw_mod_synth_create_task(ysw_bus_t *bus, ysw_mod_host_t *mod_host);

void ysw_mod_generate_samples(ysw_mod_synth_t *ysw_mod_synth,
        int16_t *buffer, uint32_t number, ysw_mod_sample_type_t sample_type);
