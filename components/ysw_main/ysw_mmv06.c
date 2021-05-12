// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#if defined(MM_VERSION) && MM_VERSION == MM_V06

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

#define TAG "YSW_MMV06"

#define MOD_SYNTH_I2S 1
#define FLUID_SYNTH_I2S 2

#define AUDIO_TYPE MOD_SYNTH_I2S

#include "board.h"
#include "ac101.h"

// mmv06 is wired to facilitate PCB layout

// 11, 10,  9      100, 107, 114
//  8,  7,  6      101, 108, 115
//  5,  4,  3      102, 109, 116
//  0,  1,  2      103, 110, 117
// 14, 13, 12      104, 111, 118
// 15, 16, 17      105, 112, 119
// 26, 25, 24      106, 113, 120

//  35,  34,  33,  32,  31,  30,  29,  28,  27,  18,  19,  20,  21,  22,  23
// -60, -62, -64, -65, -67, -69, -71, -72, -74, -76, -77, -79, -81, -83, -84

static const ysw_mapper_item_t mmv06_map[] = {
    /*  0 */ 103,
    /*  1 */ 110,
    /*  2 */ 117,
    /*  3 */ 116,
    /*  4 */ 109,
    /*  5 */ 102,
    /*  6 */ 115,
    /*  7 */ 108,
    /*  8 */ 101,
    /*  9 */ 114,
    /* 10 */ 107,
    /* 11 */ 100,
    /* 12 */ 118,
    /* 13 */ 111,
    /* 14 */ 104,
    /* 15 */ 105,
    /* 16 */ 112,
    /* 17 */ 119,
    /* 18 */ -76,
    /* 19 */ -77,
    /* 20 */ -79,
    /* 21 */ -81,
    /* 22 */ -83,
    /* 23 */ -84,
    /* 24 */ 120,
    /* 25 */ 113,
    /* 26 */ 106,
    /* 27 */ -74,
    /* 28 */ -72,
    /* 29 */ -71,
    /* 30 */ -69,
    /* 31 */ -67,
    /* 32 */ -65,
    /* 33 */ -64,
    /* 34 */ -62,
    /* 35 */ -60,
};

#if AUDIO_TYPE == MOD_SYNTH_I2S
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
#endif

static void initialize_audio_output(void)
{
    // Enable Ai Thinker esp32-audio-kit onboard AC101 Codec
    ESP_LOGD(TAG, "Enabling Ai Thinker esp32-audio-kit onboard AC101 Codec");

    audio_board_init();

    ac101_ctrl_state(AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX,
        .sample_rate = YSW_I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM,
        .dma_buf_count = YSW_I2S_BUFFER_COUNT,
        .dma_buf_len = YSW_I2S_SAMPLE_COUNT,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
    };

    $(i2s_driver_install(0, &i2s_config, 0, NULL));

    i2s_pin_config_t pin_config = { };

    get_i2s_pins(0, &pin_config);

    $(i2s_set_pin(0, &pin_config));

    i2s_mclk_gpio_select(0, GPIO_NUM_0);

    ESP_LOGW(TAG, "****** Turning Speaker Off *******");
    extern esp_err_t ac101_set_spk_volume(int volume);
	$(ac101_set_spk_volume(0));
}

static void initialize_touch_screen(void)
{
    ESP_LOGD(TAG, "configuring Music Machine v06 touch screen");
    eli_ili9341_xpt2046_config_t new_config = {
        .mosi = 12,
        .clk = 23,
        .ili9341_cs = 13,
        .xpt2046_cs = 15,
        .dc = 19,
        .rst = -1,
        .bckl = -1,
        .miso = 14,
        .irq = 22,
        .x_min = 353,
        .y_min = 313,
        .x_max = 3829,
        .y_max = 3853,
        .is_invert_y = false,
        .spi_host = VSPI_HOST,
        .is_log_min_max = true,
    };
    eli_ili9341_xpt2046_initialize(&new_config);
}

void ysw_main_init_device(ysw_bus_t *bus)
{
    initialize_touch_screen();
    //ysw_tty_create_task(bus);
	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = 5,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_io_num = 18,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 100000,
	};
    ysw_i2c_t *i2c = ysw_i2c_create(I2C_NUM_1, &i2c_config, YSW_I2C_EXCLUSIVE);
    ysw_array_t *addresses = ysw_array_load(3, 0x5a, 0x5c, 0x5d); // GND, SDA, SCL
    ysw_touch_create_task(bus, i2c, addresses);
    ysw_mapper_create_task(bus, mmv06_map);
}

void ysw_main_init_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
#if AUDIO_TYPE == MOD_SYNTH_I2S
    ysw_mod_synth = ysw_mod_synth_create_task(bus);
    // TODO: consider whether we can initialize audio output in this task before launching the i2s task
    // I think the previous approach is a remnant of the common main logic
    ysw_i2s_create_task(initialize_audio_output, generate_audio);
#elif AUDIO_TYPE == FLUID_SYNTH_I2S
    initialize_audio_output();
    ysw_fluid_synth_create_task(bus, YSW_MUSIC_SOUNDFONT, "i2s");
#endif
}

#endif
