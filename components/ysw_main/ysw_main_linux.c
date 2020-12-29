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

#include "ysw_editor.h"
#include "ysw_alsa.h"
#include "ysw_event.h"
#include "ysw_fluid_synth.h"
#include "ysw_heap.h"
#include "ysw_midi.h"
#include "ysw_sequencer.h"
#include "ysw_staff.h"
#include "ysw_mod_music.h"
#include "ysw_mod_synth.h"
#include "ysw_simulator.h"
#include "zm_music.h"
#include "lvgl.h"
#include "lv_drivers/display/monitor.h"
#include "lv_drivers/indev/mouse.h"
#include "lv_examples/lv_examples.h"
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
    ysw_fluid_synth_create_task(bus, YSW_MUSIC_SOUNDFONT);
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

    ysw_mod_host_t *mod_host = ysw_mod_music_create_host(music);
    ysw_mod_synth = ysw_mod_synth_create_task(bus, mod_host);
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

void ysw_main_init_device(ysw_bus_t *bus)
{
    initialize_touch_screen();
    ysw_simulator_initialize(bus);
}

void ysw_main_init_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
    initialize_mod_synthesizer(bus, music);
}

//#define YSW_TEST 1

int main(int argc, char *argv[])
{
#if YSW_TEST
    extern void ysw_test_all();
    ysw_test_all();
#endif
    extern void ysw_main_create();
    ysw_main_create();

    return 0;
}

#endif
