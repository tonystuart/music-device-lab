// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_editor.h"
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

// Music Machine v01 - Confirmed with Schematic

#include "eli_ili9341_xpt2046.h"
#include "lvgl/lvgl.h"
#include "driver/spi_master.h"
#include "esp_log.h"

static void initialize_touch_screen(void)
{
    ESP_LOGD(TAG, "main: configuring Music Machine v01");
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

#elif YSW_MAIN_DISPLAY_MODEL == 2

// Music Machine v02

#include "eli_ili9341_xpt2046.h"
#include "lvgl/lvgl.h"
#include "driver/spi_master.h"
#include "esp_log.h"

static void initialize_touch_screen(void)
{
#if 0
    ESP_LOGD(TAG, "main: waiting for display to power up");
    ysw_wait_millis(1000);
#endif

    ESP_LOGD(TAG, "main: configuring Music Machine v02");
    eli_ili9341_xpt2046_config_t new_config = {
        .mosi = 5,
        .clk = 18,
        .ili9341_cs = 0,
        .xpt2046_cs = 22,
        .dc = 2,
        .rst = -1,
        .bckl = -1,
        .miso = 19,
        .irq = 21,
        .x_min = 483,
        .y_min = 416,
        .x_max = 3829,
        .y_max = 3655,
        .is_invert_y = true,
        .spi_host = VSPI_HOST,
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

    //lv_task_create(memory_monitor, 5000, LV_TASK_PRIO_MID, NULL);
}

#else

#error Define YSW_MAIN_DISPLAY_MODEL

#endif


static zm_song_t *initialize_song(ysw_bus_h bus, zm_music_t *music, uint32_t index)
{
    zm_song_t *song = ysw_array_get(music->songs, index);
    zm_small_t part_count = ysw_array_get_count(song->parts);

    for (zm_small_t i = 0; i < part_count; i++) {
        zm_part_t  *part = ysw_array_get(song->parts, i);
        zm_sample_t *sample = ysw_array_get(music->samples, part->pattern->sample_index);
        ysw_event_sample_load_t sample_load = {
            .index = part->pattern->sample_index,
            .reppnt = sample->reppnt,
            .replen = sample->replen,
            .volume = sample->volume,
            .pan = sample->pan,
        };
        snprintf(sample_load.name, sizeof(sample_load.name),
                "%s/samples/%s", YSW_MUSIC_PARTITION, sample->name);
        ysw_event_fire_sample_load(bus, &sample_load);
    };
    return song;
}


#ifdef IDF_VER
#include "ysw_keyboard.h"
#include "ysw_led.h"
#include "ysw_spiffs.h"
void app_main()
{
    esp_log_level_set("efuse", ESP_LOG_INFO);
    esp_log_level_set("TRACE_HEAP", ESP_LOG_INFO);
    ysw_spiffs_initialize(YSW_MUSIC_PARTITION);
#else
#include <ysw_simulator.h>
int main(int argc, char *argv[])
{
#endif

    ysw_bus_h bus = ysw_event_create_bus();

    initialize_touch_screen();
    initialize_synthesizer(bus);

    ysw_sequencer_create_task(bus);
    ysw_editor_create_task(bus);

#ifdef IDF_VER
    ysw_led_config_t led_config = {
        .gpio = 4,
    };
    ysw_led_create_task(bus, &led_config);

    ysw_keyboard_config_t keyboard_config = {
        .rows = ysw_array_load(6, 33, 32, 35, 34, 39, 36),
        .columns = ysw_array_load(7, 15, 13, 12, 14, 27, 26, 23),
    };
    ysw_keyboard_create_task(bus, &keyboard_config);
#endif

    zm_music_t *music = zm_read();
    zm_song_t *song = initialize_song(bus, music, 0);
    ysw_array_t *notes = zm_render_song(song);

    //ysw_event_fire_loop(bus, true);
    //ysw_event_fire_play(bus, notes, song->bpm);

#ifdef IDF_VER
#else
    ysw_simulator_initialize(bus);
    pthread_exit(0);
#endif
}

