// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_event.h"
#include "ysw_bus.h"
#include "esp_log.h"

#define TAG "EVENT"

ysw_bus_h ysw_event_create_bus()
{
    return ysw_bus_create(YSW_ORIGIN_LAST, 4, 16, sizeof(ysw_event_t));
}

void ysw_event_publish(ysw_bus_h bus, ysw_event_t *event)
{
    ysw_bus_publish(bus, event->header.origin, event, sizeof(ysw_event_t));
}

void ysw_event_fire_note_on(ysw_bus_h bus, uint8_t channel, uint8_t midi_note, uint8_t velocity)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_NOTE,
        .header.type = YSW_EVENT_NOTE_ON,
        .note_on.channel = channel,
        .note_on.midi_note = midi_note,
        .note_on.velocity = velocity,
    };
    ysw_event_publish(bus, &event);
}

void ysw_event_fire_note_off(ysw_bus_h bus, uint8_t channel, uint8_t midi_note)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_NOTE,
        .header.type = YSW_EVENT_NOTE_OFF,
        .note_off.channel = channel,
        .note_off.midi_note = midi_note,
    };
    ysw_event_publish(bus, &event);
}

void ysw_event_fire_program_change(ysw_bus_h bus, uint8_t channel, uint8_t program)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_NOTE,
        .header.type = YSW_EVENT_PROGRAM_CHANGE,
        .program_change.channel = channel,
        .program_change.program = program,
    };
    ysw_event_publish(bus, &event);
}

void ysw_event_fire_loop_done(ysw_bus_h bus)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_NOTE,
        .header.type = YSW_EVENT_LOOP_DONE,
    };
    ysw_event_publish(bus, &event);
}

void ysw_event_fire_play_done(ysw_bus_h bus)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_NOTE,
        .header.type = YSW_EVENT_PLAY_DONE,
    };
    ysw_event_publish(bus, &event);
}

void ysw_event_fire_idle(ysw_bus_h bus)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_NOTE,
        .header.type = YSW_EVENT_IDLE,
    };
    ysw_event_publish(bus, &event);
}

void ysw_event_fire_note_status(ysw_bus_h bus, ysw_note_t *note)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_NOTE,
        .header.type = YSW_EVENT_NOTE_STATUS,
        .note_status.note = *note,
    };
    ysw_event_publish(bus, &event);
}

void ysw_event_fire_key_down(ysw_bus_h bus, ysw_event_key_down_t *key_down)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_KEYBOARD,
        .header.type = YSW_EVENT_KEY_DOWN,
        .key_down = *key_down,
    };
    ysw_event_publish(bus, &event);
}

void ysw_event_fire_key_pressed(ysw_bus_h bus, ysw_event_key_pressed_t *key_pressed)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_KEYBOARD,
        .header.type = YSW_EVENT_KEY_PRESSED,
        .key_pressed = *key_pressed,
    };
    ysw_event_publish(bus, &event);
}

void ysw_event_fire_key_up(ysw_bus_h bus, ysw_event_key_up_t *key_up)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_KEYBOARD,
        .header.type = YSW_EVENT_KEY_UP,
        .key_up = *key_up,
    };
    ysw_event_publish(bus, &event);
}

void ysw_event_fire_loop(ysw_bus_h bus, bool loop)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_COMMAND,
        .header.type = YSW_EVENT_LOOP,
        .loop.loop = loop,
    };
    ysw_event_publish(bus, &event);
}

void ysw_event_fire_play(ysw_bus_h bus, ysw_array_t *notes, uint8_t bpm)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_COMMAND,
        .header.type = YSW_EVENT_PLAY,
        .play.type = YSW_EVENT_PLAY_NOW,
        .play.clip.notes = notes,
        .play.clip.tempo = bpm,
    };
    ESP_LOGD(TAG, "ysw_event_fire_play event.play.tempo=%d", event.play.clip.tempo);
    ysw_event_publish(bus, &event);
}

void ysw_event_fire_sample_load(ysw_bus_h bus, ysw_event_sample_load_t *sample_load)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_SAMPLER,
        .header.type = YSW_EVENT_SAMPLE_LOAD,
        .sample_load = *sample_load,
    };
    ysw_event_publish(bus, &event);
}

