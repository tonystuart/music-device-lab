// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_bus.h"
#include "ysw_keystate.h"
#include "esp_log.h"
#include "SDL2/SDL_scancode.h"
#include "stdint.h"

#define TAG "YSW_SIMULATOR"

typedef void (*lv_key_handler)(SDL_Scancode sdl_code, uint8_t scan_code, uint32_t time, uint8_t repeat);
extern lv_key_handler lv_key_down_handler;
extern lv_key_handler lv_key_up_handler;
static ysw_keystate_t *keystate;

static void on_key_down(SDL_Scancode sdl_code, uint8_t sym, uint32_t time, uint8_t repeat)
{
    //ESP_LOGD(TAG, "on_key_down code=%d, sym=%d (%c), time=%d, repeat=%d", sdl_code, sym, sym, time, repeat);
    ysw_keystate_on_press(keystate, sdl_code);
}

static void on_key_up(SDL_Scancode sdl_code, uint8_t sym, uint32_t time, uint8_t repeat)
{
    ysw_keystate_on_release(keystate, sdl_code);
}

void ysw_simulator_initialize(ysw_bus_t *bus)
{
    keystate = ysw_keystate_create(bus, SDL_NUM_SCANCODES);
    lv_key_down_handler = on_key_down;
    lv_key_up_handler = on_key_up;
}
