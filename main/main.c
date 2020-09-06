// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#if 0
#include "ysw_main_display.h"

#include "ysw_csl.h"
#include "ysw_hpl.h"
#include "ysw_music.h"
#include "ysw_mfr.h"
#include "ysw_main_bus.h"
#include "ysw_main_seq.h"
#include "ysw_spiffs.h"
#include "ysw_style.h"
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
            ysw_csl_create(music, 0);
        } else if (strcmp(text, "Progressions") == 0) {
            ysw_hpl_create(music, 0);
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

static void play_hp_loop(ysw_music_t *music, uint32_t hp_index)
{
    ysw_seq_message_t message = {
        .type = YSW_SEQ_LOOP,
        .loop.loop = true,
    };

    ysw_main_seq_send(&message);

    ysw_hp_t *hp = ysw_music_get_hp(music, hp_index);

    uint32_t note_count = 0;
    ysw_note_t *notes = ysw_hp_get_notes(hp, &note_count);

    message = (ysw_seq_message_t){
        .type = YSW_SEQ_PLAY,
        .play.notes = notes,
        .play.note_count = note_count,
        .play.tempo = hp->tempo,
    };

    ysw_main_seq_send(&message);
}

void app_main()
{
    ESP_LOGD(TAG, "sizeof(ysw_cs_t)=%d", sizeof(ysw_cs_t));
    esp_log_level_set("spi_master", ESP_LOG_INFO);
    esp_log_level_set("BLEServer", ESP_LOG_INFO);
    esp_log_level_set("BLEDevice", ESP_LOG_INFO);
    esp_log_level_set("BLECharacteristic", ESP_LOG_INFO);
    esp_log_level_set("TRACE_HEAP", ESP_LOG_INFO);
    esp_log_level_set("YSW_HEAP", ESP_LOG_INFO);
    esp_log_level_set("YSW_ARRAY", ESP_LOG_INFO);

    ysw_spiffs_initialize(YSW_MUSIC_PARTITION);
    ysw_main_bus_create();
    ysw_main_display_initialize();
    ysw_main_synth_initialize();
    ysw_main_seq_initialize();
    ysw_style_initialize();

    music = ysw_mfr_read();

    if (music && ysw_music_get_cs_count(music) > 0) {
        create_dashboard();
        ESP_LOGI(TAG, "app_main playing hp loop");
        play_hp_loop(music, 1);
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
#else

#include "a2dp_source.h"
#include "hxcmod.h"
#include "ysw_heap.h"
#include "ysw_spiffs.h"
#include "ysw_music.h"
#include "esp_log.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "unistd.h"

#define TAG "MOD_MAIN"

static modcontext *modctx;

// NB: len is in bytes (typically 512 when called from a2dp_source)
static int32_t data_cb(uint8_t *data, int32_t len)
{
    if (len < 0 || data == NULL) {
        return 0;
    }
    hxcmod_fillbuffer(modctx, (msample *)data, len / 4, NULL);
    return len;
}

#define YSW_MUSIC_MOD YSW_MUSIC_PARTITION "/music.mod"

void app_main()
{
    ESP_LOGD(TAG, "calling ysw_spiffs_initialize, partition=%s", YSW_MUSIC_PARTITION);
    ysw_spiffs_initialize(YSW_MUSIC_PARTITION);

    ESP_LOGD(TAG, "calling ysw_heap_allocate for modcontext, size=%d", sizeof(modcontext));
    modctx = ysw_heap_allocate(sizeof(modcontext));

    ESP_LOGD(TAG, "calling hxcmod_init, modctx=%p", modctx);
    if (!hxcmod_init(modctx)) {
        ESP_LOGE(TAG, "hxcmod_init failed");
        abort();
    }

    ESP_LOGD(TAG, "calling stat, file=%s", YSW_MUSIC_MOD);
    struct stat sb;
    int rc = stat(YSW_MUSIC_MOD, &sb);
    if (rc == -1) {
        ESP_LOGE(TAG, "stat failed, file=%s", YSW_MUSIC_MOD);
        abort();
    }

    int mod_data_size = sb.st_size;

    ESP_LOGD(TAG, "calling ysw_heap_allocate for mod_data, size=%d", mod_data_size);
    void *mod_data = ysw_heap_allocate(mod_data_size);

    int fd = open(YSW_MUSIC_MOD, O_RDONLY);
    if (fd == -1) {
        ESP_LOGE(TAG, "open failed, file=%s", YSW_MUSIC_MOD);
        abort();
    }

    rc = read(fd, mod_data, mod_data_size);
    if (rc != mod_data_size) {
        ESP_LOGE(TAG, "read failed, rc=%d, mod_data_size=%d", rc, mod_data_size);
        abort();
    }

    rc = close(fd);
    if (rc == -1) {
        ESP_LOGE(TAG, "close failed");
        abort();
    }

    if (!hxcmod_load(modctx, mod_data, mod_data_size)) {
        ESP_LOGE(TAG, "hxcmod_load failed");
        abort();
    }

    a2dp_source_initialize(data_cb);
}

#endif
