// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// Device specific initialization for ESP32

#ifdef IDF_VER

#include "ysw_editor.h"
#include "ysw_bt_synth.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_fluid_synth.h"
#include "ysw_heap.h"
#include "ysw_i2s.h"
#include "ysw_keyboard.h"
#include "ysw_led.h"
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

#define TAG "YSW_MAIN_ESP32"

#define MM_V02 2 // JLCPCB board with internal DAC
#define MM_V04 4 // Wire-wrapped board with CS4344 DAC
#define MM_V05 5 // Ai Thinker esp32-audio-kit with AC101 Codec

#define MM_VERSION MM_V04

UNUSED
static void initialize_bt_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
    ESP_LOGD(TAG, "configuring BlueTooth synth");
    ysw_bt_synth_create_task(bus);
}

UNUSED
static void initialize_vs_synthesizer(ysw_bus_t *bus, zm_music_t *music)
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

UNUSED
static void initialize_fs_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
    ESP_LOGD(TAG, "configuring FluidSynth synth");
    ysw_fluid_synth_create_task(bus, YSW_MUSIC_SOUNDFONT);
}

UNUSED
static void initialize_i2s_internal_dac(void)
{
    // Enable I2S internal DAC on both GPIO 25 and GPIO 26
    ESP_LOGD(TAG, "Enabling I2S internal DAC on both GPIO 25 and GPIO 26");

    static const i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
        .sample_rate = YSW_I2S_SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = 0,
        .dma_buf_count = YSW_I2S_BUFFER_COUNT,
        .dma_buf_len = YSW_I2S_SAMPLE_COUNT,
        .use_apll = false
    };

    $(i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL));

    $(i2s_set_pin(I2S_NUM_0, NULL));
}

UNUSED
static void initialize_i2s_mmv02(void)
{
    // Enable I2S internal DAC on GPIO 25 with GPIO 26 in use for keyboard
    ESP_LOGD(TAG, "Enabling I2S internal DAC on GPIO 25 with GPIO 26 in use for keyboard");

    static const i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
        .sample_rate = YSW_I2S_SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = 0,
        .dma_buf_count = YSW_I2S_BUFFER_COUNT,
        .dma_buf_len = YSW_I2S_SAMPLE_COUNT,
        .use_apll = false
    };

    $(i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL));

    ESP_LOGD(TAG, "calling i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN)");
    $(i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN));
}

UNUSED
static void initialize_i2s_mmv04(void)
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

#include "board.h"
#include "ac101.h"

UNUSED
static void initialize_i2s_mmv05(void)
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
}

// TODO: pass ysw_mod_synth to I2S task event handlers

ysw_mod_synth_t *ysw_mod_synth;

#if MM_VERSION == MM_V02
static ysw_mod_sample_type_t sample_type = YSW_MOD_16BIT_UNSIGNED;
#elif MM_VERSION == MM_V04
static ysw_mod_sample_type_t sample_type = YSW_MOD_16BIT_SIGNED;
#elif MM_VERSION == MM_V05
static ysw_mod_sample_type_t sample_type = YSW_MOD_16BIT_SIGNED;
#else
#error define MM_VERSION
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

static void initialize_mod_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
    ESP_LOGD(TAG, "configuring MOD synth with I2S");

    ysw_mod_host_t *mod_host = ysw_mod_music_create_host(music);
    ysw_mod_synth = ysw_mod_synth_create_task(bus, mod_host);
    // ysw_a2dp_source_initialize(generate_audio); // for bluetooth speaker
#if MM_VERSION == MM_V02
    ysw_i2s_create_task(initialize_i2s_mmv02, generate_audio);
#elif MM_VERSION == MM_V04
    ysw_i2s_create_task(initialize_i2s_mmv04, generate_audio);
#elif MM_VERSION == MM_V05
    ysw_i2s_create_task(initialize_i2s_mmv05, generate_audio);
#else
#error define MM_VERSION
#endif
}

// Music Machine v01 - Confirmed with Schematic

UNUSED
static void initialize_mmv01_touch_screen(void)
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

UNUSED
static void initialize_mmv02_touch_screen(void)
{
    ESP_LOGD(TAG, "configuring Music Machine v02 touch screen");
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

UNUSED
static void initialize_mmv04_touch_screen(void)
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

UNUSED
static void initialize_mmv05_touch_screen(void)
{
    // TODO: figure out best pin assignments... note that GPIO0 may be used for MCLK
    ESP_LOGD(TAG, "configuring Music Machine v05 touch screen");
    eli_ili9341_xpt2046_config_t new_config = {
        .mosi = 13,
        .clk = 22,
        .ili9341_cs = 19,
        .xpt2046_cs = 23,
        .dc = 18,
        .rst = -1,
        .bckl = -1,
        .miso = 5,
        .irq = 21,
        .x_min = 353,
        .y_min = 313,
        .x_max = 3829,
        .y_max = 3853,
        .is_invert_y = false,
        .spi_host = VSPI_HOST,
    };
    eli_ili9341_xpt2046_initialize(&new_config);
}

void ysw_main_display_tasks()
{
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
}

void ysw_main_init_device(ysw_bus_t *bus)
{
    esp_log_level_set("efuse", ESP_LOG_INFO);
    esp_log_level_set("TRACE_HEAP", ESP_LOG_INFO);
    esp_log_level_set("I2S", ESP_LOG_INFO); // esp-idf i2s
    ysw_spiffs_initialize(ZM_MF_PARTITION);

#if MM_VERSION == MM_V02
    initialize_mmv02_touch_screen();

    ysw_led_config_t led_config = {
        .gpio = 4,
    };
    ysw_led_create_task(bus, &led_config);

    ysw_keyboard_config_t keyboard_config = {
        .rows = ysw_array_load(6, 33, 32, 35, 34, 39, 36),
        .columns = ysw_array_load(7, 15, 13, 12, 14, 27, 26, 23),
    };
    ysw_keyboard_create_task(bus, &keyboard_config);
#elif MM_VERSION == MM_V04
    initialize_mmv04_touch_screen();
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
#elif MM_VERSION == MM_V05
    initialize_mmv05_touch_screen();
    ysw_tty_create_task(bus);
#else
#error define MM_VERSION
#endif
}

void ysw_main_init_synthesizer(ysw_bus_t *bus, zm_music_t *music)
{
    initialize_mod_synthesizer(bus, music);
}

void app_main()
{
    extern void ysw_main_create();
    ysw_main_create();
}

#endif
