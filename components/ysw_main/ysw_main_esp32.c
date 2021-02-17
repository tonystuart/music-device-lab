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
#include "ysw_shell.h"
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

#ifndef MM_VERSION
#error define MM_VERSION in component.mk or equivalent
#endif

extern void ysw_main_init_device(ysw_bus_t *bus);
extern void ysw_main_init_synthesizer(ysw_bus_t *bus, zm_music_t *music);

void app_main()
{
    esp_log_level_set("efuse", ESP_LOG_INFO);
    esp_log_level_set("TRACE_HEAP", ESP_LOG_INFO);
    esp_log_level_set("I2S", ESP_LOG_INFO); // esp-idf i2s
    ysw_spiffs_initialize(ZM_MF_PARTITION);

    ysw_bus_t *bus = ysw_event_create_bus();
    ysw_main_init_device(bus);
    zm_music_t *music = zm_load_music();
    ysw_main_init_synthesizer(bus, music);
    ysw_sequencer_create_task(bus);
    ysw_shell_create(bus, music);
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

#endif
