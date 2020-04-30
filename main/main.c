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
#include "ysw_lv_cse.h"
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

static ysw_array_t *csn_clipboard;

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

static void initialize_sequencer()
{
    synthesizer_queue = ysw_ble_synthesizer_create_task();

    ysw_sequencer_config_t config = {
        .on_note_on = on_note_on,
        .on_note_off = on_note_off,
        .on_program_change = on_program_change,
        .on_state_change = on_state_change,
    };

    sequencer_queue = ysw_sequencer_create_task(&config);
}

static void play_progression(ysw_progression_t *s)
{
    if (!sequencer_queue) {
        return;
    }

    note_t *notes = ysw_progression_get_notes(s);
    assert(notes);

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
    };

    ysw_message_send(sequencer_queue, &message);
}

static void copy_to_csn_clipboard(ysw_csn_t *csn)
{
    ysw_csn_t *new_csn = ysw_csn_copy(csn);
    ysw_csn_select(csn, false);
    ysw_csn_select(new_csn, true);
    ysw_array_push(csn_clipboard, new_csn);
}

static void decrease_volume(ysw_csn_t *csn)
{
    int new_velocity = ((csn->velocity - 1) / 10) * 10;
    if (new_velocity >= 0) {
        csn->velocity = new_velocity;
    }
}

static void increase_volume(ysw_csn_t *csn)
{
    int new_velocity = ((csn->velocity + 10) / 10) * 10;
    if (new_velocity <= 120) {
        csn->velocity = new_velocity;
    }
}

static void decrease_pitch(ysw_csn_t *csn)
{
    if (csn->degree > -21) {
        if (ysw_csn_is_sharp(csn)) {
            ysw_csn_set_natural(csn);
        } else if (ysw_csn_is_natural(csn)) {
            ysw_csn_set_flat(csn);
        } else if (ysw_csn_is_flat(csn)) {
            ysw_csn_set_natural(csn);
            csn->degree--;
        }
    }
}

static void increase_pitch(ysw_csn_t *csn)
{
    if (csn->degree < 21) {
        if (ysw_csn_is_flat(csn)) {
            ysw_csn_set_natural(csn);
        } else if (ysw_csn_is_natural(csn)) {
            ysw_csn_set_sharp(csn);
        } else if (ysw_csn_is_sharp(csn)) {
            ysw_csn_set_natural(csn);
            csn->degree++;
        }
    }
}

static void decrease_duration(ysw_csn_t *csn)
{
    int new_duration = ((csn->duration - 1) / 5) * 5;
    if (new_duration >= 5) {
        csn->duration = new_duration;
    }
}

static void increase_duration(ysw_csn_t *csn)
{
    int new_duration = ((csn->duration + 5) / 5) * 5;
    if (new_duration <= 400) {
        csn->duration = new_duration;
    }
}

static void decrease_start(ysw_csn_t *csn)
{
    int new_start = ((csn->start - 1) / 5) * 5;
    if (new_start >= 0) {
        csn->start = new_start;
    }
}

static void increase_start(ysw_csn_t *csn)
{
    int new_start = ((csn->start + 5) / 5) * 5;
    if (new_start + csn->duration <= 400) {
        csn->start = new_start;
    }
}

typedef void (*note_visitor_t)(ysw_csn_t *csn);

static void visit_notes(note_visitor_t visitor, lv_event_t event)
{
    if (event == LV_EVENT_PRESSED || event == LV_EVENT_LONG_PRESSED_REPEAT) {
        ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
        uint32_t csn_count = ysw_cs_get_csn_count(cs);
        for (uint32_t i = 0; i < csn_count; i++) {
            ysw_csn_t *csn = ysw_cs_get_csn(cs, i);
            if (ysw_csn_is_selected(csn)) {
                visitor(csn);
            }
        }
        ysw_csef_redraw(csef);
    }
}

static void on_next(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_RELEASED) {
        uint32_t cs_count = ysw_music_get_cs_count(music);
        if (++cs_index >= cs_count) {
            cs_index = 0;
        }
        ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
        ysw_csef_set_cs(csef, cs);
    }
}

static void on_play(lv_obj_t * btn, lv_event_t event)
{
}

static void on_pause(lv_obj_t * btn, lv_event_t event)
{
}

static void on_loop(lv_obj_t * btn, lv_event_t event)
{
    if (!sequencer_queue) {
        return;
    }

    if (event == LV_EVENT_PRESSED) {
        if (!lv_btn_get_toggle(btn)) {
            // first press
            lv_btn_set_toggle(btn, true);
        }
        if (lv_btn_get_state(btn) == LV_BTN_STATE_TGL_PR) {
            ysw_sequencer_message_t message = {
                .type = YSW_SEQUENCER_SET_LOOP,
                .set_loop.loop = false,
            };
            ysw_message_send(sequencer_queue, &message);
        } else {
            ysw_sequencer_message_t message = {
                .type = YSW_SEQUENCER_SET_LOOP,
                .set_loop.loop = true,
            };
            ysw_message_send(sequencer_queue, &message);
        }
    }
}

static void on_prev(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_RELEASED) {
        uint32_t cs_count = ysw_music_get_cs_count(music);
        if (--cs_index >= cs_count) {
            cs_index = cs_count - 1;
        }
        ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
        ysw_csef_set_cs(csef, cs);
    }
}

static void on_close(lv_obj_t * btn, lv_event_t event)
{
}

static void on_settings(lv_obj_t * btn, lv_event_t event)
{
}

static void on_save(lv_obj_t * btn, lv_event_t event)
{
}

static void on_copy(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_PRESSED) {
        if (csn_clipboard) {
            uint32_t old_csn_count = ysw_array_get_count(csn_clipboard);
            for (int i = 0; i < old_csn_count; i++) {
                ysw_csn_t *old_csn = ysw_array_get(csn_clipboard, i);
                ysw_csn_free(old_csn);
            }
            ysw_array_truncate(csn_clipboard, 0);
        } else {
            csn_clipboard = ysw_array_create(10);
        }
        visit_notes(copy_to_csn_clipboard, event);
    }
}

static void on_paste(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_PRESSED) {
        if (csn_clipboard) {
            ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
            uint32_t csn_count = ysw_array_get_count(csn_clipboard);
            for (uint32_t i = 0; i < csn_count; i++) {
                ysw_csn_t *csn = ysw_array_get(csn_clipboard, i);
                ysw_csn_t *new_csn = ysw_csn_copy(csn);
                new_csn->state = csn->state;
                ysw_cs_add_csn(cs, new_csn);
            }
            ysw_csef_redraw(csef);
        }
    }
}

static void on_volume_mid(lv_obj_t * btn, lv_event_t event)
{
    visit_notes(decrease_volume, event);
}

static void on_volume_max(lv_obj_t * btn, lv_event_t event)
{
    visit_notes(increase_volume, event);
}

static void on_up(lv_obj_t * btn, lv_event_t event)
{
    visit_notes(increase_pitch, event);
}

static void on_down(lv_obj_t * btn, lv_event_t event)
{
    visit_notes(decrease_pitch, event);
}

static void on_plus(lv_obj_t * btn, lv_event_t event)
{
    visit_notes(increase_duration, event);
}

static void on_minus(lv_obj_t * btn, lv_event_t event)
{
    visit_notes(decrease_duration, event);
}

static void on_left(lv_obj_t * btn, lv_event_t event)
{
    visit_notes(decrease_start, event);
}

static void on_right(lv_obj_t * btn, lv_event_t event)
{
    visit_notes(increase_start, event);
}

static void on_trash(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_PRESSED) {
        uint32_t target = 0;
        uint32_t changes = 0;
        ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
        uint32_t csn_count = ysw_cs_get_csn_count(cs);
        for (uint32_t source = 0; source < csn_count; source++) {
            ysw_csn_t *csn = ysw_array_get(cs->csns, source);
            if (ysw_csn_is_selected(csn)) {
                ysw_csn_free(csn);
                changes++;
            } else {
                ysw_array_set(cs->csns, target, csn);
                target++;
            }
        }
        if (changes) {
            ysw_array_truncate(cs->csns, target);
            ysw_csef_redraw(csef);
        }
    }
}

static void cse_event_cb(lv_obj_t *ysw_lv_cse, ysw_lv_cse_event_t event, ysw_lv_cse_event_cb_data_t *data)
{
    if (event == YSW_LV_CSE_SELECT) {
    } else if (event == YSW_LV_CSE_DESELECT) {
    } else if (event == YSW_LV_CSE_DOUBLE_CLICK) {
        ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
        ysw_csn_t *csn = ysw_csn_create(data->double_click.degree, 80, data->double_click.start, 80, 0);
        ysw_cs_add_csn(cs, csn);
        ysw_csef_redraw(csef);
    }
}

#if 0
static bool csef_event_cb(lv_obj_t *ysw_csef, ysw_csef_event_t event, ysw_csef_event_cb_data_t *data)
{
}
#endif

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

    //initialize_sequencer();

    ysw_lv_styles_initialize();

    music = ysw_music_parse(MUSIC_DEFINITIONS);

    if (music) {
        ysw_csef_config_t config = {
            .next_cb = on_next,
            .play_cb = on_play,
            .pause_cb = on_pause,
            .loop_cb = on_loop,
            .prev_cb = on_prev,
            .close_cb = on_close,
            .settings_cb = on_settings,
            .save_cb = on_save,
            .copy_cb = on_copy,
            .paste_cb = on_paste,
            .volume_mid_cb = on_volume_mid,
            .volume_max_cb = on_volume_max,
            .up_cb = on_up,
            .down_cb = on_down,
            .plus_cb = on_plus,
            .minus_cb = on_minus,
            .left_cb = on_left,
            .right_cb = on_right,
            .trash_cb = on_trash,
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
