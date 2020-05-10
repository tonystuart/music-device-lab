// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_csc.h" // TODO: main or not?
#include "ysw_main_display.h"
#include "ysw_main_sequencer.h"
#include "ysw_main_spiffs.h"
#include "ysw_main_synthesizer.h"
#include "ysw_music.h"
#include "ysw_music_parser.h"
#include "ysw_lv_styles.h"
#include "lvgl/lvgl.h"
#include "esp_log.h"

#define TAG "MAIN"

#define SPIFFS_PARTITION "/spiffs"
#define MUSIC_DEFINITIONS "/spiffs/music.csv"

static ysw_music_t *music;

void app_main()
{
    ESP_LOGD(TAG, "sizeof(ysw_cs_t)=%d", sizeof(ysw_cs_t));
    esp_log_level_set("spi_master", ESP_LOG_INFO);
    esp_log_level_set("BLEServer", ESP_LOG_INFO);
    esp_log_level_set("BLEDevice", ESP_LOG_INFO);
    esp_log_level_set("BLECharacteristic", ESP_LOG_INFO);
    esp_log_level_set("YSW_HEAP", ESP_LOG_INFO);
    esp_log_level_set("YSW_ARRAY", ESP_LOG_INFO);

    ysw_main_display_initialize();
    ysw_main_spiffs_initialize(SPIFFS_PARTITION);
    ysw_main_synthesizer_initialize();
    ysw_main_sequencer_initialize();

    ysw_lv_styles_initialize();

    music = ysw_music_parse(MUSIC_DEFINITIONS);

    if (music) {
        ysw_main_csc_initialize(music);
    } else {
        lv_obj_t * mbox1 = lv_mbox_create(lv_scr_act(), NULL);
        lv_mbox_set_text(mbox1, "The music partition is empty");
        lv_obj_set_width(mbox1, 200);
        lv_obj_align(mbox1, NULL, LV_ALIGN_CENTER, 0, 0);
    }

    while (1) {
        wait_millis(1);
        lv_task_handler();
    }
}
