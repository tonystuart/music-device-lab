// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "chord_style.h"
#include "display.h"
#include "sequencer.h"
#include "spiffs.h"
#include "synthesizer.h"

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

    display_initialize();
    spiffs_initialize(SPIFFS_PARTITION);
    synthesizer_initialize();
    sequencer_initialize();

    ysw_lv_styles_initialize();

    music = ysw_music_parse(MUSIC_DEFINITIONS);

    if (music && ysw_music_get_cs_count(music) > 0) {
        edit_chord_styles(music, 0);
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
