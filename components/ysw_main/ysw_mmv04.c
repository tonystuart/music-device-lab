// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#if defined(MM_VERSION) && MM_VERSION == MM_V04

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

#define TAG "YSW_MMV04"

#define RMT_CHANNEL_LED 0
#define RMT_CHANNEL_REMOTE 1

static const ysw_mapper_item_t mmv04_map[] = {
    /*  0 */ -60,
    /*  1 */ -61,
    /*  2 */ -62,
    /*  3 */ -63,
    /*  4 */ -64,
    /*  5 */ -65,
    /*  6 */ -66,
    /*  7 */ -67,
    /*  8 */ -68,
    /*  9 */ -69,
    /* 10 */ -70,
    /* 11 */ -71,
    /* 12 */ -72,
    /* 13 */ -73,
    /* 14 */ -74,
    /* 15 */ -75,
    /* 16 */ -76,
    /* 17 */ -77,
    /* 18 */ -78,
    /* 19 */ -79,
    /* 20 */ -80,
    /* 21 */ -81,
    /* 22 */ -82,
    /* 23 */ -83,
};

ysw_mod_synth_t *ysw_mod_synth;

static int32_t generate_audio(uint8_t *buffer, int32_t bytes_requested)
{
    int32_t bytes_generated = 0;
    if (buffer && bytes_requested) {
        ysw_mod_generate_samples(ysw_mod_synth, (int16_t*)buffer, bytes_requested / 4, YSW_MOD_16BIT_SIGNED);
        bytes_generated = bytes_requested;
    }
    return bytes_generated;
}

static void initialize_audio_output(void)
{
    // Enable I2S external Cirrus Logic CS4344 DAC using GPIO 0 for MCLK
    ESP_LOGD(TAG, "Enabling I2S external Cirrus Logic CS4344 DAC using GPIO 0 for MCLK");

// Search for CLK_OUT1 and see page 73 of ESP32 Technical Reference Manual
// See https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf
// See https://www.reddit.com/r/esp32/comments/g48lzs/esp_32_i2s_and_cs4344_dac/
// See https://www.esp32.com/viewtopic.php?f=5&t=1585&start=10

#define YSW_I2S_MCLK PERIPHS_IO_MUX_GPIO0_U // must be a clock peripheral
#define YSW_I2S_BCK_PIN 2
#define YSW_I2S_LRCK_PIN 5
#define YSW_I2S_DATA_PIN 4

    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = YSW_I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = 0,
        .dma_buf_count = YSW_I2S_BUFFER_COUNT,
        .dma_buf_len = YSW_I2S_SAMPLE_COUNT,
        .use_apll = true,
    };

    $(i2s_driver_install(0, &i2s_config, 0, NULL));

    i2s_pin_config_t pin_config = {
        .bck_io_num = YSW_I2S_BCK_PIN,
        .ws_io_num = YSW_I2S_LRCK_PIN,
        .data_out_num = YSW_I2S_DATA_PIN,
        .data_in_num = -1,
    };

    $(i2s_set_pin(0, &pin_config));

    REG_WRITE(PIN_CTRL, 0b111111110000);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
}

static void initialize_touch_screen(void)
{
    ESP_LOGD(TAG, "configuring Music Machine v04 touch screen");
    eli_ili9341_xpt2046_config_t new_config = {
        .mosi = 25,
        .clk = 27,
        .ili9341_cs = 13,
        .xpt2046_cs = 26,
        .dc = 12,
        .rst = -1,
        .bckl = 14,
        .miso = 33,
        .irq = 32,
        .x_min = 353,
        .y_min = 313,
        .x_max = 3829,
        .y_max = 3853,
        .is_invert_y = false,
        .spi_host = VSPI_HOST,
    };
    eli_ili9341_xpt2046_initialize(&new_config);
}

void ysw_main_init_device(ysw_bus_t *bus)
{
    initialize_touch_screen();

    ysw_remote_create_task(bus, RMT_CHANNEL_REMOTE, 22);

	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = 18,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_io_num = 23,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 100000,
	};
    ysw_i2c_t *i2c = ysw_i2c_create(I2C_NUM_1, &i2c_config, YSW_I2C_EXCLUSIVE);
    ysw_array_t *addresses = ysw_array_load(2, 0x5a, 0x5d);
    ysw_touch_create_task(bus, i2c, addresses);
    ysw_mapper_create_task(bus, mmv04_map);
}

void ysw_main_init_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
    ysw_mod_host_t *mod_host = ysw_mod_music_create_host(music);
    ysw_mod_synth = ysw_mod_synth_create_task(bus, mod_host);
    ysw_i2s_create_task(initialize_audio_output, generate_audio);
}

#endif
