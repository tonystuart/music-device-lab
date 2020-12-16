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
#include "ysw_midi.h"
#include "ysw_sequencer.h"
#include "ysw_staff.h"
#include "ysw_mod_music.h"
#include "ysw_mod_synth.h"
#include "zm_music.h"
#include "esp_log.h"
#include "pthread.h"
#include "stdlib.h"

#define TAG "YSW_MAIN"

#if YSW_MAIN_SYNTH_MODEL == 1 // bluetooth

#include "ysw_bt_synth.h"

void initialize_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
    ESP_LOGD(TAG, "initialize_synthesizer: configuring BlueTooth synth");
    ysw_bt_synth_create_task(bus);
}

#elif YSW_MAIN_SYNTH_MODEL == 2 // vs1053b

#include "ysw_vs_synth.h"

void initialize_synthesizer(ysw_bus_t *bus, zm_music_t *music)
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
    ysw_vs_synth_create_task(bus, &config);
}

#elif YSW_MAIN_SYNTH_MODEL == 3 // fluid synth

#include "ysw_fluid_synth.h"

void initialize_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
    ESP_LOGD(TAG, "initialize_synthesizer: configuring FluidSynth synth");
    ysw_fluid_synth_create_task(bus, YSW_MUSIC_SOUNDFONT);
}

#elif YSW_MAIN_SYNTH_MODEL == 4 // mod synth

#include "ysw_mod_synth.h"

// TODO: pass ysw_mod_synth to ALSA and I2S task event handlers

ysw_mod_synth_t *ysw_mod_synth;

#ifdef IDF_VER
static ysw_mod_sample_type_t sample_type = YSW_MOD_16BIT_UNSIGNED;
#else
static ysw_mod_sample_type_t sample_type = YSW_MOD_16BIT_SIGNED;
#endif

static int32_t generate_audio(uint8_t *buffer, int32_t bytes_requested)
{
    int32_t bytes_generated = 0;
    if (buffer && bytes_requested) {
        ysw_mod_generate_samples(ysw_mod_synth, (int16_t*)buffer, bytes_requested / 4, sample_type);
        bytes_generated = bytes_requested;
    }
    return bytes_generated;
}

#ifdef IDF_VER

#include "ysw_i2s.h"

void initialize_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
    ESP_LOGD(TAG, "initialize_synthesizer: configuring MOD synth with I2S");

    ysw_mod_host_t *mod_host = ysw_mod_music_create_host(music);
    ysw_mod_synth = ysw_mod_synth_create_task(bus, mod_host);
    // ysw_a2dp_source_initialize(generate_audio); // for bluetooth speaker
    ysw_i2s_create_task(generate_audio); // for i2s builtin dac mode
}

#else

#include "ysw_alsa.h"
#include "pthread.h"

static void* alsa_thread(void *p)
{
    ysw_alsa_initialize(generate_audio);
    return NULL;
}

void initialize_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
    ESP_LOGD(TAG, "initialize_synthesizer: configuring MOD synth with ALSA");

    ysw_mod_host_t *mod_host = ysw_mod_music_create_host(music);
    ysw_mod_synth = ysw_mod_synth_create_task(bus, mod_host);
    pthread_t p;
    pthread_create(&p, NULL, &alsa_thread, NULL);
}

#endif

#else // no synthesizer defined

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

// TODO: move lv_tick_inc to ysw_editor and remove this if it is not necessary
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

static void edit_section(ysw_bus_t *bus, zm_music_t *music)
{
    zm_section_t *section;
    if (ysw_array_get_count(music->sections) > 0) {
        section = ysw_array_get(music->sections, 0);
    } else {
        section = zm_music_create_section(music);
    }
    ysw_event_fire_section_edit(bus, section);
}

#ifdef IDF_VER
#include "ysw_keyboard.h"
#include "ysw_led.h"
#include "ysw_spiffs.h"
void app_main()
{
    esp_log_level_set("efuse", ESP_LOG_INFO);
    esp_log_level_set("TRACE_HEAP", ESP_LOG_INFO);
    ysw_spiffs_initialize(ZM_MF_PARTITION);
#else
#include <ysw_simulator.h>
int main(int argc, char *argv[])
{
#endif

    zm_music_t *music = zm_load_music();
    ysw_bus_t *bus = ysw_event_create_bus();

    initialize_synthesizer(bus, music);

    ysw_sequencer_create_task(bus);
    ysw_editor_create_task(bus, music, initialize_touch_screen);
    edit_section(bus, music);

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

#ifdef IDF_VER
#if configUSE_TRACE_FACILITY && configTASKLIST_INCLUDE_COREID
    static uint32_t last_report = 0;
    uint32_t current_millis = ysw_get_millis();
    if (current_millis > last_report + 5000) {
        TaskStatus_t *task_status = ysw_heap_allocate(30 * sizeof(TaskStatus_t));
        uint32_t count = uxTaskGetSystemState( task_status, 30, NULL );
        for (uint32_t i = 0; i < count; i++) {
            char state;
            switch( task_status[i].eCurrentState) {
                case eReady:
                    state = 'R';
                    break;
                case eBlocked:
                    state = 'B';
                    break;
                case eSuspended:
                    state = 'S';
                    break;
                case eDeleted:
                    state = 'D';
                    break;
                default:
                    state = '?';
                    break;
            }
            ESP_LOGD(TAG, "%3d %c %-20s %d %d %d %d",
                    i,
                    state,
                    task_status[i].pcTaskName,
                    task_status[i].uxCurrentPriority,
                    task_status[i].usStackHighWaterMark,
                    task_status[i].xTaskNumber,
                    task_status[i].xCoreID );
        }
        last_report = current_millis;
    }
#endif
#else
    ysw_simulator_initialize(bus);
    pthread_exit(0);
#endif
}

