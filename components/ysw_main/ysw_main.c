// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_sequencer.h"
#include "ysw_staff.h"
#include "ysw_synth_mod.h"
#include "zm_music.h"
#include "esp_log.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"
#include "termios.h"
#include "unistd.h"

#define TAG "MAIN"

#define YSW_MUSIC_PARTITION "/spiffs"

#if YSW_MAIN_SYNTH_MODEL == 1

#include "ysw_synth_bt.h"

void initialize_synthesizer(ysw_bus_h bus)
{
    ESP_LOGD(TAG, "initialize_synthesizer: configuring BlueTooth synth");
    ysw_synth_bt_create_task(bus);
}

#elif YSW_MAIN_SYNTH_MODEL == 2

#include "ysw_synth_vs.h"

void initialize_synthesizer(ysw_bus_h bus)
{
    ESP_LOGD(TAG, "initialize_synthesizer: configuring VS1053 synth");
    ysw_vs1053_config_t config = {
        .dreq_gpio = -1,
        .xrst_gpio = 0,
        .miso_gpio = 15,
        .mosi_gpio = 17,
        .sck_gpio = 2,
        .xcs_gpio = 16,
        .xdcs_gpio = 4,
        .spi_host = VSPI_HOST,
    };
    ysw_synth_vs_create_task(bus, &config);
}

#elif YSW_MAIN_SYNTH_MODEL == 3

#include "ysw_synth_fs.h"

void initialize_synthesizer(ysw_bus_h bus)
{
    ESP_LOGD(TAG, "initialize_synthesizer: configuring FluidSynth synth");
    ysw_synth_fs_create_task(bus, YSW_MUSIC_SOUNDFONT);
}

#elif YSW_MAIN_SYNTH_MODEL == 4

#include "ysw_synth_mod.h"

void initialize_synthesizer(ysw_bus_h bus)
{
    ESP_LOGD(TAG, "initialize_synthesizer: configuring MOD synth");
    ysw_synth_mod_create_task(bus);
}

#else

#error Define YSW_MAIN_SYNTH_MODEL

#endif

#if YSW_MAIN_DISPLAY_MODEL == 1

#include "eli_ili9341_xpt2046.h"
#include "lvgl/lvgl.h"
#include "driver/spi_master.h"
#include "esp_log.h"

static void initialize_display(void)
{
    ESP_LOGD(TAG, "main: configuring model 1");
    eli_ili9341_xpt2046_config_t new_config = {
        .mosi = 21,
        .clk = 22,
        .ili9341_cs = 5,
        .xpt2046_cs = 32,
        .dc = 19,
        .rst = 18,
        .bckl = 23,
        .miso = 27,
        .irq = 14,
        .x_min = 346,
        .y_min = 235,
        .x_max = 3919,
        .y_max = 3883,
        .spi_host = HSPI_HOST,
    }
    eli_ili9341_xpt2046_initialize(&new_config);
}

#elif YSW_MAIN_DISPLAY_MODEL == 2

#include "eli_ili9341_xpt2046.h"
#include "lvgl/lvgl.h"
#include "driver/spi_master.h"
#include "esp_log.h"

static void initialize_display(void)
{
    ESP_LOGD(TAG, "main: configuring model 2");
    eli_ili9341_xpt2046_config_t new_config = {
        .mosi = 21,
        .clk = 19,
        .ili9341_cs = 23,
        .xpt2046_cs = 5,
        .dc = 22,
        .rst = -1,
        .bckl = -1,
        .miso = 18,
        .irq = 13,
        .x_min = 346,
        .y_min = 235,
        .x_max = 3919,
        .y_max = 3883,
        .spi_host = HSPI_HOST,
    };
    eli_ili9341_xpt2046_initialize(&new_config);
}

#elif YSW_MAIN_DISPLAY_MODEL == 3

#include "lvgl.h"
#include "lv_drivers/display/monitor.h"
#include <SDL2/SDL.h>

#include <stdlib.h>
#include <unistd.h>
#include "lvgl/lvgl.h"
#include "lv_drivers/display/monitor.h"
#include "lv_drivers/indev/mouse.h"
#include "lv_examples/lv_examples.h"
#include "esp_log.h"

static int tick_thread(void *data)
{
    while (1) {
        SDL_Delay(5); /*Sleep for 5 millisecond*/
        lv_tick_inc(5); /*Tell LittelvGL that 5 milliseconds were elapsed*/
    }

    return 0;
}

static void initialize_display(void)
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

    //lv_task_create(memory_monitor, 5000, LV_TASK_PRIO_MID, NULL);
}

#else

#error Define YSW_MAIN_DISPLAY_MODEL

#endif


static void fire_loop(ysw_bus_h bus, bool loop)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_COMMAND,
        .header.type = YSW_EVENT_LOOP,
        .loop.loop = loop,
    };
    ysw_event_publish(bus, &event);
}

static void fire_play(ysw_bus_h bus, ysw_array_t *notes, uint8_t bpm)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_COMMAND,
        .header.type = YSW_EVENT_PLAY,
        .play.notes = notes,
        .play.tempo = bpm,
    };
    ysw_event_publish(bus, &event);
}

static void fire_sample_load(ysw_bus_h bus, zm_medium_t index, zm_sample_t *sample)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_SAMPLER,
        .header.type = YSW_EVENT_SAMPLE_LOAD,
        .sample_load.index = index,
        .sample_load.reppnt = sample->reppnt,
        .sample_load.replen = sample->replen,
        .sample_load.volume = sample->volume,
        .sample_load.pan = sample->pan,
    };
    snprintf(event.sample_load.name, sizeof(event.sample_load.name),
        "%s/samples/%s", YSW_MUSIC_PARTITION, sample->name);
    ysw_event_publish(bus, &event);
}

static zm_song_t *initialize_song(ysw_bus_h bus, zm_music_t *music, uint32_t index)
{
    zm_song_t *song = ysw_array_get(music->songs, index);
    zm_small_t part_count = ysw_array_get_count(song->parts);

    for (zm_small_t i = 0; i < part_count; i++) {
        zm_part_t  *part = ysw_array_get(song->parts, i);
        zm_sample_t *sample = ysw_array_get(music->samples, part->pattern->sample_index);
        fire_sample_load(bus, part->pattern->sample_index, sample);
    }

    return song;
}


static void play_song()
{
    initialize_display();

    //lv_obj_t *staff = ysw_staff_create(lv_scr_act(), NULL);
    //lv_obj_set_size(staff, 320, 240);
    //lv_obj_align(staff, NULL, LV_ALIGN_CENTER, 0, 0);

    ysw_bus_h bus = ysw_event_create_bus();

    initialize_synthesizer(bus);
    ysw_sequencer_create_task(bus);

    zm_music_t *music = zm_read();
    zm_song_t *song = initialize_song(bus, music, 0);
    ysw_array_t *notes = zm_render_song(song);

    //ysw_staff_set_notes(staff, notes);

    fire_loop(bus, true);
    fire_play(bus, notes, song->bpm);
}

#ifdef IDF_VER

#include "ysw_spiffs.h"

void app_main()
{
    ysw_spiffs_initialize(YSW_MUSIC_PARTITION);

    play_song();
}

#else

#include "ysw_lvgl.h"

int main(int argc, char *argv[])
{
    play_song();

    while (1) {
        lv_task_handler();
        usleep(5 * 1000);
    }
}

#endif


