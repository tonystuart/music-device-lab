// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// Device specific initialization for Linux

#ifndef IDF_VER

#include "ysw_alsa.h"
#include "ysw_app.h"
#include "ysw_event.h"
#include "ysw_fluid_synth.h"
#include "ysw_heap.h"
#include "ysw_mapper.h"
#include "ysw_midi.h"
#include "ysw_sequencer.h"
#include "ysw_mod_synth.h"
#include "ysw_shell.h"
#include "ysw_simulator.h"
#include "zm_music.h"
#include "lvgl.h"
#include "lv_drivers/display/monitor.h"
#include "lv_drivers/indev/mouse.h"
#include "esp_log.h"
#include "pthread.h"
#include "SDL2/SDL.h"
#include "stdlib.h"
#include "unistd.h"

#define TAG "YSW_MAIN_LINUX"

UNUSED
static void initialize_fs_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
    ESP_LOGD(TAG, "configuring FluidSynth synth");
    ysw_fluid_synth_create_task(bus, YSW_MUSIC_SOUNDFONT, "alsa");
}

// TODO: pass ysw_mod_synth to ALSA task event handlers

ysw_mod_synth_t *ysw_mod_synth;

static ysw_mod_sample_type_t sample_type = YSW_MOD_16BIT_SIGNED;

static int32_t generate_audio(uint8_t *buffer, int32_t bytes_requested)
{
    int32_t bytes_generated = 0;
    if (buffer && bytes_requested) {
        ysw_mod_generate_samples(ysw_mod_synth, (int16_t*)buffer, bytes_requested / 4, sample_type);
        bytes_generated = bytes_requested;
    }
    return bytes_generated;
}

static void* alsa_thread(void *p)
{
    ysw_alsa_initialize(generate_audio);
    return NULL;
}

static void initialize_mod_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
    ESP_LOGD(TAG, "configuring MOD synth with ALSA");

    ysw_mod_synth = ysw_mod_synth_create_task(bus);
    pthread_t p;
    pthread_create(&p, NULL, &alsa_thread, NULL);
}

static int tick_thread(void *data)
{
    while (1) {
        SDL_Delay(5);
        lv_tick_inc(5);
    }
    return 0;
}

static void initialize_touch_screen(void)
{
    lv_init();
    monitor_init();

    static lv_disp_buf_t disp_buf1;
    static lv_color_t buf1_1[LV_HOR_RES_MAX * 120];
    lv_disp_buf_init(&disp_buf1, buf1_1, NULL, LV_HOR_RES_MAX * 120);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.buffer = &disp_buf1;
    disp_drv.flush_cb = monitor_flush;
    lv_disp_drv_register(&disp_drv);

    mouse_init();
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv); /*Basic initialization*/
    indev_drv.type = LV_INDEV_TYPE_POINTER;

    indev_drv.read_cb = mouse_read;
    lv_indev_t *mouse_indev = lv_indev_drv_register(&indev_drv);

    LV_IMG_DECLARE(mouse_cursor_icon); /*Declare the image file.*/
    lv_obj_t *cursor_obj = lv_img_create(lv_scr_act(), NULL); /*Create an image object for the cursor */
    lv_img_set_src(cursor_obj, &mouse_cursor_icon); /*Set the image source*/
    lv_indev_set_cursor(mouse_indev, cursor_obj); /*Connect the image  object to the driver*/

    SDL_CreateThread(tick_thread, "tick", NULL);
}

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
//   T2, T3,     T5, T6, T7,     LK,  /,  *, UP,
//  Q,  W,  E,  R,  T,  Y,  U,   N7, N8, N9, DN,
//    S,  D,      G,  H,  I,     N4, N5, N6, LT,
//  Z,  X,  C,  V,  B,  N,  M,   N1, N2, N3, RT,
//

static const ysw_mapper_item_t mmv02_map[SDL_NUM_SCANCODES] = {
    [ SDL_SCANCODE_2] = -73, // C#
    [ SDL_SCANCODE_3] = -75, // D#
    [ SDL_SCANCODE_5] = -78, // F#
    [ SDL_SCANCODE_6] = -80, // G#
    [ SDL_SCANCODE_7] = -82, // A#

    [ SDL_SCANCODE_Q] = -72, // C
    [ SDL_SCANCODE_W] = -74, // D
    [ SDL_SCANCODE_E] = -76, // E
    [ SDL_SCANCODE_R] = -77, // F
    [ SDL_SCANCODE_T] = -79, // G
    [ SDL_SCANCODE_Y] = -81, // A
    [ SDL_SCANCODE_U] = -83, // B

    [ SDL_SCANCODE_S] = -61, // C#
    [ SDL_SCANCODE_D] = -63, // D#
    [ SDL_SCANCODE_G] = -66, // F#
    [ SDL_SCANCODE_H] = -68, // G#
    [ SDL_SCANCODE_J] = -70, // A#

    [ SDL_SCANCODE_Z] = -60, // C
    [ SDL_SCANCODE_X] = -62, // D
    [ SDL_SCANCODE_C] = -64, // E
    [ SDL_SCANCODE_V] = -65, // F
    [ SDL_SCANCODE_B] = -67, // G
    [ SDL_SCANCODE_N] = -69, // A
    [ SDL_SCANCODE_M] = -71, // B

    [ SDL_SCANCODE_RIGHT] = YSW_R4_C4, // Right Arrow
    [ SDL_SCANCODE_LEFT] = YSW_R3_C4, // Left Arrow
    [ SDL_SCANCODE_DOWN] = YSW_R2_C4, // Down Arrow
    [ SDL_SCANCODE_UP] = YSW_R1_C4, // Up Arrow

    [ SDL_SCANCODE_NUMLOCKCLEAR] = YSW_R1_C1, // Keypad Num Lock
    [ SDL_SCANCODE_KP_DIVIDE] = YSW_R1_C2, // Keypad Slash (/)
    [ SDL_SCANCODE_KP_MULTIPLY] = YSW_R1_C3, // Keypad Asterisk (*)

    [ SDL_SCANCODE_KP_MINUS] = YSW_R1_C4, // Keypad Minus (-) -> Same as UP
    [ SDL_SCANCODE_KP_PLUS] = YSW_R2_C4, // Keypad Plus (+) -> Same as DOWN
    [ SDL_SCANCODE_KP_ENTER] = YSW_R4_C4, // Keypad Enter    -> Same as RIGHT

    [ SDL_SCANCODE_KP_1] = YSW_R4_C1, // N1
    [ SDL_SCANCODE_KP_2] = YSW_R4_C2, // N2
    [ SDL_SCANCODE_KP_3] = YSW_R4_C3, // N3

    [ SDL_SCANCODE_KP_4] = YSW_R3_C1, // N4
    [ SDL_SCANCODE_KP_5] = YSW_R3_C2, // N5
    [ SDL_SCANCODE_KP_6] = YSW_R3_C3, // N6

    [ SDL_SCANCODE_KP_7] = YSW_R2_C1, // N7
    [ SDL_SCANCODE_KP_8] = YSW_R2_C2, // N8
    [ SDL_SCANCODE_KP_9] = YSW_R2_C3, // N9

    [ SDL_SCANCODE_KP_0] = YSW_R3_C4, // Keypad 0/Insert  -> Same as LEFT
    [ SDL_SCANCODE_KP_PERIOD] = YSW_R3_C2, // Keypad ./Delete    -> Same as DELETE

    [ SDL_SCANCODE_DELETE] = YSW_R3_C2, // Delete Button
};

void ysw_main_init_device(ysw_bus_t *bus)
{
    initialize_touch_screen();
    ysw_simulator_initialize(bus);
    ysw_mapper_create_task(bus, mmv02_map);
}

void ysw_main_init_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
    //initialize_fs_synthesizer(bus, music);
    initialize_mod_synthesizer(bus, music);
}

//#define YSW_TEST 1
//#define YSW_EXTRACTOR 1

int main(int argc, char *argv[])
{
#if YSW_TEST
    extern void ysw_test_all();
    ysw_test_all();
#elif YSW_EXTRACTOR
    extern int extract(int argc, char *argv[]);
    char *args[] = {"extract", "extractor/music.sf2", "extractor/tmp"};
    extract(3, args);
#else
    ysw_bus_t *bus = ysw_event_create_bus();
    ysw_main_init_device(bus);
    zm_music_t *music = zm_load_music();
    ysw_main_init_synthesizer(bus, music);
    ysw_sequencer_create_task(bus);
    ysw_shell_create(bus, music);
    return 0;
#endif
}

#endif
