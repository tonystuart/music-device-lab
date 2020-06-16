// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_main_display.h"

#include "ysw_csc.h"
#include "ysw_csl.h"
#include "ysw_gsc.h"
#include "ysw_hpc.h"
#include "ysw_music.h"
#include "ysw_mfr.h"
#include "ysw_main_seq.h"
#include "ysw_spiffs.h"
#include "ysw_styles.h"
#include "ysw_main_synth.h"

#include "lvgl/lvgl.h"

#include "esp_log.h"

#define TAG "MAIN"

static ysw_music_t *music;

static void event_handler(lv_obj_t *btn, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        const char *text = lv_list_get_btn_text(btn);
        if (strcmp(text, "Chords") == 0) {
            //ysw_csc_create(music, 0);
            ysw_csl_create(music);
        } else if (strcmp(text, "Progressions") == 0) {
            ysw_hpc_create(music, 0);
        } else if (strcmp(text, "Globals") == 0) {
            ysw_gsc_create(music);
        }
    }
}

static void create_dashboard(void)
{
    lv_obj_t *dashboard = lv_list_create(lv_scr_act(), NULL);
    lv_obj_set_size(dashboard, 160, 200);
    lv_obj_align(dashboard, NULL, LV_ALIGN_CENTER, 0, 0);

    /*Add buttons to the list*/
    lv_obj_t *list_btn;

    list_btn = lv_list_add_btn(dashboard, LV_SYMBOL_AUDIO, "Chords");
    lv_obj_set_event_cb(list_btn, event_handler);

    list_btn = lv_list_add_btn(dashboard, LV_SYMBOL_BELL, "Progressions");
    lv_obj_set_event_cb(list_btn, event_handler);

    list_btn = lv_list_add_btn(dashboard, LV_SYMBOL_IMAGE, "Globals");
    lv_obj_set_event_cb(list_btn, event_handler);
}

void app_main()
{
    ESP_LOGD(TAG, "sizeof(ysw_cs_t)=%d", sizeof(ysw_cs_t));
    esp_log_level_set("spi_master", ESP_LOG_INFO);
    esp_log_level_set("BLEServer", ESP_LOG_INFO);
    esp_log_level_set("BLEDevice", ESP_LOG_INFO);
    esp_log_level_set("BLECharacteristic", ESP_LOG_INFO);
    esp_log_level_set("YSW_HEAP", ESP_LOG_INFO);
    esp_log_level_set("YSW_ARRAY", ESP_LOG_INFO);

    ysw_spiffs_initialize(YSW_MUSIC_PARTITION);
    ysw_main_display_initialize();
    ysw_main_synth_initialize();
    ysw_main_seq_initialize();
    ysw_styles_initialize();

    music = ysw_mfr_read();

    if (music && ysw_music_get_cs_count(music) > 0) {
        //create_dashboard();
        extern int create_mockup();
        create_mockup();
    } else {
        lv_obj_t *mbox1 = lv_msgbox_create(lv_scr_act(), NULL);
        lv_msgbox_set_text(mbox1, "The music partition is empty");
        lv_obj_set_width(mbox1, 200);
        lv_obj_align(mbox1, NULL, LV_ALIGN_CENTER, 0, 0);
    }

    while (1) {
        wait_millis(1);
        lv_task_handler();
    }
}
