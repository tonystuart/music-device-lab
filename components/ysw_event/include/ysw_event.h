// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_array.h"
#include "ysw_bus.h"
#include "ysw_common.h"
#include "ysw_note.h"
#include "ysw_origin.h"
#include "zm_music.h"

typedef enum {
    YSW_EVENT_PLAY,
    YSW_EVENT_PAUSE,
    YSW_EVENT_RESUME,
    YSW_EVENT_STOP,
    YSW_EVENT_TEMPO,
    YSW_EVENT_LOOP,
    YSW_EVENT_SPEED,
    YSW_EVENT_NOTE_ON,
    YSW_EVENT_NOTE_OFF,
    YSW_EVENT_PROGRAM_CHANGE,
    YSW_EVENT_SAMPLE_LOAD,
    YSW_EVENT_LOOP_DONE,
    YSW_EVENT_PLAY_DONE,
    YSW_EVENT_IDLE,
    YSW_EVENT_NOTE_STATUS,
    YSW_EVENT_KEY_DOWN,
    YSW_EVENT_KEY_PRESSED,
    YSW_EVENT_KEY_UP,
} ysw_event_type_t;

typedef struct {
    ysw_origin_t origin;
    ysw_event_type_t type;
} ysw_event_header_t;

typedef struct {
    ysw_array_t *notes; // must remain accessible for duration of playback
    uint8_t bpm;
} ysw_event_clip_t;

typedef enum {
    YSW_EVENT_PLAY_NOW, // play now, replacing anything that is playing
    YSW_EVENT_PLAY_STAGE, // play now if nothing playing, otherwise clear play list and add
    YSW_EVENT_PLAY_QUEUE, // play now if nothing playing, otherwise add to play list
} ysw_event_play_type_t;

typedef struct {
    ysw_event_play_type_t type;
    ysw_event_clip_t clip;
} ysw_event_play_t;

typedef struct {
    uint8_t bpm;
} ysw_event_tempo_t;

typedef struct {
    bool loop;
} ysw_event_loop_t;

typedef struct {
    uint8_t percent;
} ysw_event_speed_t;

typedef struct {
    uint8_t channel;
    uint8_t midi_note;
    uint8_t velocity;
} ysw_event_note_on_t;

typedef struct {
    uint8_t channel;
    uint8_t midi_note;
} ysw_event_note_off_t;

typedef struct {
    uint8_t channel;
    uint8_t program;
} ysw_event_program_change_t;

typedef struct {
    ysw_note_t note;
} ysw_event_note_status_t;

typedef struct {
    char name[YSW_COMMON_FS_NAME_MAX];
    uint16_t index;
    uint16_t reppnt;
    uint16_t replen;
    uint8_t volume;
    uint8_t pan;
} ysw_event_sample_load_t;

typedef struct {
    uint8_t key;
    uint32_t time;
    uint32_t duration;
} ysw_event_key_down_t;

typedef struct {
    uint8_t key;
    uint32_t time;
    uint32_t duration;
    uint32_t repeat_count;
} ysw_event_key_pressed_t;

typedef struct {
    uint8_t key;
    uint32_t time;
    uint32_t duration;
    uint32_t repeat_count;
} ysw_event_key_up_t;

typedef struct {
    ysw_event_header_t header;
    union {
        ysw_event_play_t play;
        ysw_event_tempo_t tempo;
        ysw_event_loop_t loop;
        ysw_event_speed_t speed;
        ysw_event_note_status_t note_status;
        ysw_event_note_on_t note_on;
        ysw_event_note_off_t note_off;
        ysw_event_program_change_t program_change;
        ysw_event_sample_load_t sample_load;
        ysw_event_key_down_t key_down;
        ysw_event_key_pressed_t key_pressed;
        ysw_event_key_up_t key_up;
    };
} ysw_event_t;

ysw_bus_h ysw_event_create_bus();
void ysw_event_publish(ysw_bus_h bus, ysw_event_t *event);

void ysw_event_fire_note_on(ysw_bus_h bus, ysw_origin_t origin, ysw_event_note_on_t *note_on);
void ysw_event_fire_note_off(ysw_bus_h bus, ysw_origin_t origin, ysw_event_note_off_t *note_off);
void ysw_event_fire_program_change(ysw_bus_h bus, ysw_origin_t origin, ysw_event_program_change_t *program);
void ysw_event_fire_loop_done(ysw_bus_h bus);
void ysw_event_fire_play_done(ysw_bus_h bus);
void ysw_event_fire_idle(ysw_bus_h bus);
void ysw_event_fire_note_status(ysw_bus_h bus, ysw_note_t *note);
void ysw_event_fire_key_down(ysw_bus_h bus, ysw_event_key_down_t *key_down);
void ysw_event_fire_key_pressed(ysw_bus_h bus, ysw_event_key_pressed_t *key_pressed);
void ysw_event_fire_key_up(ysw_bus_h bus, ysw_event_key_up_t *key_up);
void ysw_event_fire_loop(ysw_bus_h bus, bool loop);
void ysw_event_fire_play(ysw_bus_h bus, ysw_array_t *notes, uint8_t bpm);
void ysw_event_fire_sample_load(ysw_bus_h bus, ysw_event_sample_load_t *sample_load);

