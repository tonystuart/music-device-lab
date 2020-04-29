// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "esp_log.h"
#include "esp_spiffs.h"

#include "lvgl/lvgl.h"
#include "driver/spi_master.h"
#include "eli_ili9341_xpt2046.h"

#include "ysw_synthesizer.h"
#include "ysw_ble_synthesizer.h"
#include "ysw_sequencer.h"
#include "ysw_message.h"
#include "ysw_chord.h"
#include "ysw_music.h"
#include "ysw_music_parser.h"
#include "ysw_lv_styles.h"
#include "ysw_lv_cne.h"
#include "ysw_csef.h"

#define TAG "MAIN"

#define SPIFFS_PARTITION "/spiffs"
#define MUSIC_DEFINITIONS "/spiffs/music.csv"


static QueueHandle_t synthesizer_queue;
static QueueHandle_t sequencer_queue;

static ysw_csef_t *csef;
static ysw_music_t *music;
static uint32_t cs_index;
static uint32_t progression_index;

static void play_progression(ysw_progression_t *s);

static void initialize_spiffs()
{
    esp_vfs_spiffs_conf_t config = {
        .base_path = SPIFFS_PARTITION,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    $(esp_vfs_spiffs_register(&config));

    size_t total_size = 0;
    size_t amount_used = 0;
    $(esp_spiffs_info(config.partition_label, &total_size, &amount_used));
    ESP_LOGD(TAG, "initialize_spiffs total_size=%d, amount_used=%d", total_size, amount_used);
}

static void play_next()
{
    ESP_LOGI(TAG, "play_next progression_index=%d", progression_index);
    if (music && progression_index < ysw_array_get_count(music->progressions)) {
        ysw_progression_t *progression = ysw_array_get(music->progressions, progression_index);
        play_progression(progression);
        progression_index++;
    }
}

static void on_note_on(uint8_t channel, uint8_t midi_note, uint8_t velocity)
{
    ysw_synthesizer_message_t message = {
        .type = YSW_SYNTHESIZER_NOTE_ON,
        .note_on.channel = channel,
        .note_on.midi_note = midi_note,
        .note_on.velocity = velocity,
    };
    ysw_message_send(synthesizer_queue, &message);
}

static void on_note_off(uint8_t channel, uint8_t midi_note)
{
    ysw_synthesizer_message_t message = {
        .type = YSW_SYNTHESIZER_NOTE_OFF,
        .note_off.channel = channel,
        .note_off.midi_note = midi_note,
    };
    ysw_message_send(synthesizer_queue, &message);
}

static void on_program_change(uint8_t channel, uint8_t program)
{
    ysw_synthesizer_message_t message = {
        .type = YSW_SYNTHESIZER_PROGRAM_CHANGE,
        .program_change.channel = channel,
        .program_change.program = program,
    };
    ysw_message_send(synthesizer_queue, &message);
}

static void on_state_change(ysw_sequencer_state_t new_state)
{
    if (new_state == YSW_SEQUENCER_STATE_PLAYBACK_COMPLETE) {
        play_next();
    }
}

static void play_progression(ysw_progression_t *s)
{
    note_t *notes = ysw_progression_get_notes(s);
    assert(notes);

    synthesizer_queue = ysw_ble_synthesizer_create_task();

    ysw_sequencer_config_t config = {
        .on_note_on = on_note_on,
        .on_note_off = on_note_off,
        .on_program_change = on_program_change,
        .on_state_change = on_state_change,
    };

    sequencer_queue = ysw_sequencer_create_task(&config);

    ysw_sequencer_message_t message = {
        .type = YSW_SEQUENCER_INITIALIZE,
        .initialize.notes = notes,
        .initialize.note_count = ysw_progression_get_note_count(s),
    };

    ysw_message_send(sequencer_queue, &message);

    for (int i = 20; i > 0; i--) {
        ESP_LOGW(TAG, "%d - please connect the synthesizer", i);
        wait_millis(1000);
    }

    message = (ysw_sequencer_message_t){
        .type = YSW_SEQUENCER_PLAY,
            .play.loop_count = 1
    };

    ysw_message_send(sequencer_queue, &message);
}

void select_next_cs(lv_obj_t * btn, lv_event_t event)
{
    if(event == LV_EVENT_RELEASED) {
        uint32_t cs_count = ysw_music_get_cs_count(music);
        if (++cs_index >= cs_count) {
            cs_index = 0;
        }
        ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
        ysw_csef_set_cs(csef, cs);
    }
}

void select_prev_cs(lv_obj_t * btn, lv_event_t event)
{
    if(event == LV_EVENT_RELEASED) {
        uint32_t cs_count = ysw_music_get_cs_count(music);
        if (--cs_index >= cs_count) {
            cs_index = cs_count - 1;
        }
        ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
        ysw_csef_set_cs(csef, cs);
    }
}

static void display_csn_index(uint8_t csn_index)
{
    char footer_text[MDBUF_SZ];
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
    uint32_t csn_count = ysw_cs_get_csn_count(cs);
    snprintf(footer_text, MDBUF_SZ, "%d of %d", to_count(csn_index), csn_count);
    ysw_csef_set_footer_text(csef, footer_text);
}

static void cse_event_cb(lv_obj_t *ysw_lv_cse, ysw_lv_cse_event_t event, ysw_lv_cse_event_cb_data_t *data)
{
    switch (event) {
        case YSW_LV_CSE_SELECT:
            display_csn_index(data->select.index);
            break;

        case YSW_LV_CSE_DESELECT:
            ysw_csef_set_footer_text(csef, "");
            break;

        case YSW_LV_CSE_DOUBLE_CLICK:
            ESP_LOGD(TAG, "cse_event_cb double_click.start=%d", data->double_click.start);
            ESP_LOGD(TAG, "cse_event_cb double_click.degree=%d", data->double_click.degree);
            break;
    }
}

void app_main()
{
    esp_log_level_set("spi_master", ESP_LOG_INFO);

    esp_log_level_set("BLEServer", ESP_LOG_INFO);
    esp_log_level_set("BLEDevice", ESP_LOG_INFO);
    esp_log_level_set("BLECharacteristic", ESP_LOG_INFO);

    esp_log_level_set("YSW_HEAP", ESP_LOG_INFO);
    esp_log_level_set("YSW_ARRAY", ESP_LOG_INFO);

    eli_ili9341_xpt2046_config_t new_config = {
        .mosi = 21,
        .clk = 22,
        .ili9341_cs = 5,
        .xpt2046_cs = 32,
        .dc = 19,
        .rst = 18,
        .bckl = 23,
        .miso = 27,
        .irq = 14,
        .x_min = 346,
        .y_min = 235,
        .x_max = 3919,
        .y_max = 3883,
        .spi_host = HSPI_HOST,
    };

    eli_ili9341_xpt2046_initialize(&new_config);

    initialize_spiffs();
    ysw_lv_styles_initialize();

    music = ysw_music_parse(MUSIC_DEFINITIONS);

    if (music) {
        ysw_csef_config_t config = {
            .next_cb = select_next_cs,
            .prev_cb = select_prev_cs,
            .cse_event_cb = cse_event_cb,
        };
        csef = ysw_csef_create(&config);
        ysw_cs_t *cs = ysw_music_get_cs(music, 0);
        ysw_csef_set_cs(csef, cs);
    } else {
        lv_obj_t * mbox1 = lv_mbox_create(lv_scr_act(), NULL);
        lv_mbox_set_text(mbox1, "The music partition is empty");
        lv_obj_set_width(mbox1, 200);
        lv_obj_align(mbox1, NULL, LV_ALIGN_CENTER, 0, 0);
    }

    // play_next();

    while (1) {
        wait_millis(1);
        lv_task_handler();
    }
}
