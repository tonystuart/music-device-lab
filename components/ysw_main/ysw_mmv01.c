// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#if defined(MM_VERSION) && MM_VERSION == MM_V01

#include "ysw_editor.h"
#include "ysw_app.h"
#include "ysw_bt_synth.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_fluid_synth.h"
#include "ysw_heap.h"
#include "ysw_i2s.h"
#include "ysw_remote.h"
#include "ysw_keyboard.h"
#include "ysw_led.h"
#include "ysw_mapper.h"
#include "ysw_midi.h"
#include "ysw_mod_synth.h"
#include "ysw_mod_music.h"
#include "ysw_sequencer.h"
#include "ysw_staff.h"
#include "ysw_spiffs.h"
#include "ysw_touch.h"
#include "ysw_tty.h"
#include "ysw_vs_synth.h"
#include "zm_music.h"
#include "eli_ili9341_xpt2046.h"
#include "lvgl/lvgl.h"
#include "driver/gpio.h"
#include "driver/dac.h"
#include "driver/i2c.h"
#include "driver/i2s.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "stdlib.h"

#define TAG "YSW_MMV01"

// Music Machine v01 - Confirmed with Schematic

static void initialize_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
    ESP_LOGD(TAG, "configuring VS1053 synth");
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

static void initialize_touch_screen(void)
{
    ESP_LOGD(TAG, "configuring Music Machine v01 touch screen");
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

void ysw_main_init_device(ysw_bus_t *bus)
{
    initialize_touch_screen();
}

void ysw_main_init_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
    initialize_synthesizer(bus, music);
}

#endif
