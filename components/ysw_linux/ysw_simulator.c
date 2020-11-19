// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_bus.h"
#include "ysw_event.h"
#include "esp_log.h"
#include "stdint.h"

#define TAG "YSW_SIMULATOR"

typedef void (*lv_key_handler)(uint8_t key, uint32_t time, uint8_t repeat);
extern lv_key_handler lv_key_down_handler;
extern lv_key_handler lv_key_up_handler;
static uint32_t down_time;
static ysw_bus_h cb_bus;

// Layout of keycodes generated by keyboard:
//
//    0,  1,      2,  3,  4,      5,  6,  7,  8,
//  9, 10, 11, 12, 13, 14, 15,   16, 17, 18, 19,
//   20, 21,     22, 23, 24,     25, 26, 27, 28,
// 29, 30, 31, 32, 33, 34, 35,   36, 37, 38, 39,

// We try to map to a PC keyboard as closely as possible
// NB: digits are in phone order on Music Machine v02 and keypad order on PC keyboard
//
// T = top row of digits
// N = numeric keypad digits
//
//   T2, T3,     T4, T5, T6,     LK,  /,  *, UP,
//  Q,  W,  E,  R,  T,  Y,  Y,   N7, N8, N9, DN,
//    S,  D,      G,  H,  I,     N4, N5, N6, LT,
//  Z,  X,  C,  V,  B,  N,  M,   N1, N2, N3, RT,
//

typedef struct {
    char input;
    uint8_t key;
} keycode_map_t;

static const keycode_map_t keycode_map[] = {
    { '2', 0 }, // C#
    { '3', 1 }, // D#
    { '5', 2 }, // F#
    { '6', 3 }, // G#
    { '7', 4 }, // A#

    { 'q', 9 }, // C
    { 'w', 10 }, // D
    { 'e', 11 }, // E
    { 'r', 12 }, // F
    { 't', 13 }, // G
    { 'y', 14 }, // A
    { 'u', 15 }, // B

    { 's', 20 }, // C#
    { 'd', 21 }, // D#
    { 'g', 22 }, // F#
    { 'h', 23 }, // G#
    { 'j', 24 }, // A#

    { 'z', 29 }, // C
    { 'x', 30 }, // D
    { 'c', 31 }, // E
    { 'v', 32 }, // F
    { 'b', 33 }, // G
    { 'n', 34 }, // A
    { 'm', 35 }, // B

//    0,  1,      2,  3,  4,      5,  6,  7,  8,
//  9, 10, 11, 12, 13, 14, 15,   16, 17, 18, 19,
//   20, 21,     22, 23, 24,     25, 26, 27, 28,
// 29, 30, 31, 32, 33, 34, 35,   36, 37, 38, 39,

//   T2, T3,     T4, T5, T6,     LK,  /,  *, UP,
//  Q,  W,  E,  R,  T,  Y,  Y,   N7, N8, N9, DN,
//    S,  D,      G,  H,  I,     N4, N5, N6, LT,
//  Z,  X,  C,  V,  B,  N,  M,   N1, N2, N3, RT,

    { 79, 39 }, // Right Arrow
    { 80, 28 }, // Left Arrow
    { 81, 19 }, // Down Arrow
    { 82, 8 }, // Up Arrow

    { 83, 5 }, // Keypad Num Lock
    { 84, 6 }, // Keypad Slash (/)
    { 85, 7 }, // Keypad Asterisk (*)

    { 86, 8 }, // Keypad Minus (-) -> Same as UP
    { 87, 19 }, // Keypad Plus (+) -> Same as DOWN
    { 88, 39 }, // Keypad Enter    -> Same as RIGHT

    { 89, 36 }, // N1
    { 90, 37 }, // N2
    { 91, 38 }, // N3

    { 92, 25 }, // N4
    { 93, 26 }, // N5
    { 94, 27 }, // N6

    { 95, 16 }, // N7
    { 96, 17 }, // N8
    { 97, 18 }, // N9

    { 98, 28 }, // Keypad 0/Insert  -> Same as LEFT
    { 99, 25 }, // Keypad Delete    -> Same as DELETE

    { 127, 25 }, // Delete Button
};

#define KEYCODE_MAP_SZ (sizeof(keycode_map) / sizeof(keycode_map[0]))

static int find_key(char input)
{
    for (int i = 0; i < KEYCODE_MAP_SZ; i++) {
        if (keycode_map[i].input == input) {
            return keycode_map[i].key;
        }
    }
    return -1;
}

static void on_key_down(uint8_t code, uint32_t time, uint8_t repeat)
{
    ESP_LOGD(TAG, "on_key_down code=%d (%c), time=%d, repeat=%d", code, code, time, repeat);
    int key = find_key(code);
    if (key != -1) {
        if (repeat) {
            // TODO: add state to provide accurate time, duration, repeat_count -- if neccessary
            uint32_t current_millis = ysw_get_millis();
            uint32_t duration = current_millis - down_time;
            ysw_event_key_pressed_t key_pressed = {
                .key = key,
                .time = down_time,
                .duration = duration,
                .repeat_count = repeat,
            };
            ysw_event_fire_key_pressed(cb_bus, &key_pressed);
        } else {
            down_time = ysw_get_millis(); // doesn't support n-key rollover
            ysw_event_key_down_t key_down = {
                .key = key,
                .time = down_time,
            };
            ysw_event_fire_key_down(cb_bus, &key_down);
        }
    }
}

static void on_key_up(uint8_t code, uint32_t time, uint8_t repeat)
{
    //ESP_LOGD(TAG, "on_key_up code=%d (%c), time=%d, repeat=%d", code, code, time, repeat);
    int key = find_key(code);
    if (key != -1) {
        uint32_t current_millis = ysw_get_millis();
        uint32_t duration = current_millis - down_time;
        ysw_event_key_pressed_t key_pressed = {
            .key = key,
            .time = current_millis,
            .duration = duration,
            .repeat_count = 0,
        };
        ysw_event_fire_key_pressed(cb_bus, &key_pressed);
        ysw_event_key_up_t key_up = {
            .key = key,
            .time = current_millis,
            .duration = duration,
            .repeat_count = 0,
        };
        ysw_event_fire_key_up(cb_bus, &key_up);
    }
}

void ysw_simulator_initialize(ysw_bus_h bus)
{
    cb_bus = bus;
    lv_key_down_handler = on_key_down;
    lv_key_up_handler = on_key_up;
}
