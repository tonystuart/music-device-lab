// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_keystate.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "esp_log.h"

#define TAG "YSW_KEYSTATE"

void ysw_keystate_on_press(ysw_keystate_t *keystate, uint8_t scan_code)
{
    if (scan_code < keystate->size) {
        ysw_keystate_state_t *state = &keystate->state[scan_code];
        uint32_t current_millis = ysw_get_millis();
        if (!state->down_time) {
            state->repeat_count = 0;
            state->down_time = current_millis;
            ysw_event_key_down_t key_down = {
                .scan_code = scan_code,
                .time = state->down_time,
            };
            ysw_event_fire_key_down(keystate->bus, &key_down);
        } else if (state->down_time + ((state->repeat_count + 1) * 100) < current_millis) {
            state->repeat_count++;
            ysw_event_key_pressed_t key_pressed = {
                .scan_code = scan_code,
                .time = state->down_time,
                .duration = current_millis - state->down_time,
                .repeat_count = state->repeat_count,
            };
            ysw_event_fire_key_pressed(keystate->bus, &key_pressed);
        }
    }
}

void ysw_keystate_on_release(ysw_keystate_t *keystate, uint8_t scan_code)
{
    if (scan_code < keystate->size) {
        ysw_keystate_state_t *state = &keystate->state[scan_code];
        if (state->down_time) {
            uint32_t current_millis = ysw_get_millis();
            uint32_t duration = current_millis - state->down_time;
            if (!state->repeat_count) {
                ysw_event_key_pressed_t key_pressed = {
                    .scan_code = scan_code,
                    .time = state->down_time,
                    .duration = duration,
                    .repeat_count = state->repeat_count,
                };
                ysw_event_fire_key_pressed(keystate->bus, &key_pressed);
            }
            ysw_event_key_up_t key_up = {
                .scan_code = scan_code,
                .time = state->down_time,
                .duration = duration,
                .repeat_count = state->repeat_count,
            };
            ysw_event_fire_key_up(keystate->bus, &key_up);
            state->down_time = 0;
        }
    }
}

ysw_keystate_t *ysw_keystate_create(ysw_bus_t *bus, uint8_t size)
{
    ysw_keystate_t *keystate = ysw_heap_allocate(
            sizeof(ysw_keystate_t) + (sizeof(ysw_keystate_state_t) * size));

    keystate->bus = bus;
    keystate->size = size;

    return keystate;
}

void ysw_keystate_free(ysw_keystate_t *keystate)
{
    ysw_heap_free(keystate);
}

