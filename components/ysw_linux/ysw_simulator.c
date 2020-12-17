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
#include "SDL2/SDL_scancode.h"
#include "stdint.h"

#define TAG "YSW_SIMULATOR"

typedef void (*lv_key_handler)(SDL_Scancode sdl_code, uint8_t scan_code, uint32_t time, uint8_t repeat);
extern lv_key_handler lv_key_down_handler;
extern lv_key_handler lv_key_up_handler;
static uint32_t down_time;
static ysw_bus_t *cb_bus;

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
    uint8_t scan_code;
} keycode_map_t;

static const keycode_map_t scan_code_map[] = {
    { SDL_SCANCODE_2, 0 }, // C#
    { SDL_SCANCODE_3, 1 }, // D#
    { SDL_SCANCODE_5, 2 }, // F#
    { SDL_SCANCODE_6, 3 }, // G#
    { SDL_SCANCODE_7, 4 }, // A#

    { SDL_SCANCODE_Q, 9 }, // C
    { SDL_SCANCODE_W, 10 }, // D
    { SDL_SCANCODE_E, 11 }, // E
    { SDL_SCANCODE_R, 12 }, // F
    { SDL_SCANCODE_T, 13 }, // G
    { SDL_SCANCODE_Y, 14 }, // A
    { SDL_SCANCODE_U, 15 }, // B

    { SDL_SCANCODE_S, 20 }, // C#
    { SDL_SCANCODE_D, 21 }, // D#
    { SDL_SCANCODE_G, 22 }, // F#
    { SDL_SCANCODE_H, 23 }, // G#
    { SDL_SCANCODE_J, 24 }, // A#

    { SDL_SCANCODE_Z, 29 }, // C
    { SDL_SCANCODE_X, 30 }, // D
    { SDL_SCANCODE_C, 31 }, // E
    { SDL_SCANCODE_V, 32 }, // F
    { SDL_SCANCODE_B, 33 }, // G
    { SDL_SCANCODE_N, 34 }, // A
    { SDL_SCANCODE_M, 35 }, // B

//    0,  1,      2,  3,  4,      5,  6,  7,  8,
//  9, 10, 11, 12, 13, 14, 15,   16, 17, 18, 19,
//   20, 21,     22, 23, 24,     25, 26, 27, 28,
// 29, 30, 31, 32, 33, 34, 35,   36, 37, 38, 39,

//   T2, T3,     T4, T5, T6,     LK,  /,  *, UP,
//  Q,  W,  E,  R,  T,  Y,  Y,   N7, N8, N9, DN,
//    S,  D,      G,  H,  I,     N4, N5, N6, LT,
//  Z,  X,  C,  V,  B,  N,  M,   N1, N2, N3, RT,

    { SDL_SCANCODE_RIGHT, 39 }, // Right Arrow
    { SDL_SCANCODE_LEFT, 28 }, // Left Arrow
    { SDL_SCANCODE_DOWN, 19 }, // Down Arrow
    { SDL_SCANCODE_UP, 8 }, // Up Arrow

    { SDL_SCANCODE_NUMLOCKCLEAR, 5 }, // Keypad Num Lock
    { SDL_SCANCODE_KP_DIVIDE, 6 }, // Keypad Slash (/)
    { SDL_SCANCODE_KP_MULTIPLY, 7 }, // Keypad Asterisk (*)

    { SDL_SCANCODE_KP_MINUS, 8 }, // Keypad Minus (-) -> Same as UP
    { SDL_SCANCODE_KP_PLUS, 19 }, // Keypad Plus (+) -> Same as DOWN
    { SDL_SCANCODE_KP_ENTER, 39 }, // Keypad Enter    -> Same as RIGHT

    { SDL_SCANCODE_KP_1, 36 }, // N1
    { SDL_SCANCODE_KP_2, 37 }, // N2
    { SDL_SCANCODE_KP_3, 38 }, // N3

    { SDL_SCANCODE_KP_4, 25 }, // N4
    { SDL_SCANCODE_KP_5, 26 }, // N5
    { SDL_SCANCODE_KP_6, 27 }, // N6

    { SDL_SCANCODE_KP_7, 16 }, // N7
    { SDL_SCANCODE_KP_8, 17 }, // N8
    { SDL_SCANCODE_KP_9, 18 }, // N9

    { SDL_SCANCODE_KP_0, 28 }, // Keypad 0/Insert  -> Same as LEFT
    { SDL_SCANCODE_KP_PERIOD, 37 }, // Keypad ./Delete    -> Same as DELETE

    { SDL_SCANCODE_DELETE, 37 }, // Delete Button
};

#define KEYCODE_MAP_SZ (sizeof(scan_code_map) / sizeof(scan_code_map[0]))

static int find_scan_code(char input)
{
    for (int i = 0; i < KEYCODE_MAP_SZ; i++) {
        if (scan_code_map[i].input == input) {
            return scan_code_map[i].scan_code;
        }
    }
    return -1;
}

static void on_key_down(SDL_Scancode sdl_code, uint8_t sym, uint32_t time, uint8_t repeat)
{
    //ESP_LOGD(TAG, "on_key_down code=%d, sym=%d (%c), time=%d, repeat=%d", sdl_code, sym, sym, time, repeat);
    int scan_code = find_scan_code(sdl_code);
    if (scan_code != -1) {
        if (repeat) {
            // TODO: add state to provide accurate time, duration, repeat_count -- if neccessary
            uint32_t current_millis = ysw_get_millis();
            uint32_t duration = current_millis - down_time;
            ysw_event_key_pressed_t key_pressed = {
                .scan_code = scan_code,
                .time = down_time,
                .duration = duration,
                .repeat_count = repeat,
            };
            ysw_event_fire_key_pressed(cb_bus, &key_pressed);
        } else {
            down_time = ysw_get_millis(); // doesn't support n-key rollover
            ysw_event_key_down_t key_down = {
                .scan_code = scan_code,
                .time = down_time,
            };
            ysw_event_fire_key_down(cb_bus, &key_down);
        }
    }
}

static void on_key_up(SDL_Scancode sdl_code, uint8_t sym, uint32_t time, uint8_t repeat)
{
    int scan_code = find_scan_code(sdl_code);
    if (scan_code != -1) {
        uint32_t current_millis = ysw_get_millis();
        uint32_t duration = current_millis - down_time;
        ysw_event_key_pressed_t key_pressed = {
            .scan_code = scan_code,
            .time = current_millis,
            .duration = duration,
            .repeat_count = 0,
        };
        ysw_event_fire_key_pressed(cb_bus, &key_pressed);
        ysw_event_key_up_t key_up = {
            .scan_code = scan_code,
            .time = current_millis,
            .duration = duration,
            .repeat_count = 0,
        };
        ysw_event_fire_key_up(cb_bus, &key_up);
    }
}

void ysw_simulator_initialize(ysw_bus_t *bus)
{
    cb_bus = bus;
    lv_key_down_handler = on_key_down;
    lv_key_up_handler = on_key_up;
}
