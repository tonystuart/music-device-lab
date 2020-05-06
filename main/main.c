// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#define MODEL 2

#if MODEL == 1
#include "ysw_ble_synthesizer.h"
#elif MODEL == 2
#include "ysw_vs1053_synthesizer.h"
#endif

#include "esp_log.h"
#include "esp_spiffs.h"

#include "lvgl/lvgl.h"
#include "driver/spi_master.h"
#include "eli_ili9341_xpt2046.h"

#include "ysw_synthesizer.h"
#include "ysw_sequencer.h"
#include "ysw_message.h"
#include "ysw_csn.h"
#include "ysw_cs.h"
#include "ysw_music.h"
#include "ysw_music_parser.h"
#include "ysw_lv_styles.h"
#include "ysw_lv_cse.h"
#include "ysw_csf.h"
#include "ysw_sdb.h"
#include "ysw_instruments.h"
#include "ysw_octaves.h"
#include "ysw_modes.h"
#include "ysw_transpositions.h"

#define TAG "MAIN"

#define SPIFFS_PARTITION "/spiffs"
#define MUSIC_DEFINITIONS "/spiffs/music.csv"

static QueueHandle_t synthesizer_queue;
static QueueHandle_t sequencer_queue;

static ysw_csf_t *csf;
static ysw_music_t *music;

static uint32_t cs_index;

static ysw_array_t *csn_clipboard;

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

static void send_sequencer_message(ysw_sequencer_message_t *message)
{
    if (sequencer_queue) {
        ysw_message_send(sequencer_queue, message);
    }
}

static void send_synthesizer_message(ysw_synthesizer_message_t *message)
{
    if (synthesizer_queue) {
        ysw_message_send(synthesizer_queue, message);
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
    send_synthesizer_message(&message);
}

static void on_note_off(uint8_t channel, uint8_t midi_note)
{
    ysw_synthesizer_message_t message = {
        .type = YSW_SYNTHESIZER_NOTE_OFF,
        .note_off.channel = channel,
        .note_off.midi_note = midi_note,
    };
    send_synthesizer_message(&message);
}

static void on_program_change(uint8_t channel, uint8_t program)
{
    ysw_synthesizer_message_t message = {
        .type = YSW_SYNTHESIZER_PROGRAM_CHANGE,
        .program_change.channel = channel,
        .program_change.program = program,
    };
    send_synthesizer_message(&message);
}

static void on_state_change(ysw_sequencer_state_t new_state)
{
    if (new_state == YSW_SEQUENCER_STATE_PLAYBACK_COMPLETE) {
    }
}

static void initialize_synthesizer()
{
#if MODEL == 1
    ESP_LOGD(TAG, "initialize_synthesizer: configuring BLE synthesizer");
    synthesizer_queue = ysw_ble_synthesizer_create_task();
#elif MODEL == 2
    ESP_LOGD(TAG, "initialize_synthesizer: configuring VS1053 synthesizer");
    ysw_vs1053_synthesizer_config_t config = {
        .dreq_gpio = -1,
        .xrst_gpio = 0,
        .miso_gpio = 15,
        .mosi_gpio = 17,
        .sck_gpio = 2,
        .xcs_gpio = 16,
        .xdcs_gpio = 4,
        .spi_host = VSPI_HOST,
    };
    synthesizer_queue = ysw_vs1053_synthesizer_create_task(&config);
#endif
}

static void initialize_sequencer()
{
    ysw_sequencer_config_t config = {
        .on_note_on = on_note_on,
        .on_note_off = on_note_off,
        .on_program_change = on_program_change,
        .on_state_change = on_state_change,
    };

    sequencer_queue = ysw_sequencer_create_task(&config);
}

static void stage()
{
    uint32_t note_count = 0;
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
    ysw_cs_sort_csns(cs);
    note_t *notes = ysw_cs_get_notes(cs, &note_count);

    ysw_sequencer_message_t message = {
        .type = YSW_SEQUENCER_STAGE,
        .stage.notes = notes,
        .stage.note_count = note_count,
    };

    send_sequencer_message(&message);
}

static void pause()
{
    ysw_sequencer_message_t message = {
        .type = YSW_SEQUENCER_PAUSE,
    };

    send_sequencer_message(&message);
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

static void refresh()
{
    ysw_csf_redraw(csf);
    stage();
}

typedef void (*note_visitor_t)(ysw_csn_t *csn);

static void visit_notes(note_visitor_t visitor, lv_event_t event)
{
    if (event == LV_EVENT_PRESSED || event == LV_EVENT_LONG_PRESSED_REPEAT) {
        uint32_t visitor_count = 0;
        ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
        uint32_t csn_count = ysw_cs_get_csn_count(cs);
        for (uint32_t i = 0; i < csn_count; i++) {
            ysw_csn_t *csn = ysw_cs_get_csn(cs, i);
            if (ysw_csn_is_selected(csn)) {
                visitor(csn);
                visitor_count++;
            }
        }
        if (visitor_count) {
            ysw_csf_redraw(csf);
        }
    } else if (event == LV_EVENT_RELEASED) {
        stage();
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
        ysw_csf_set_cs(csf, cs);
    }
}

static void on_play(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_RELEASED) {
        stage();
    }
}

static void on_pause(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_RELEASED) {
        pause();
    }
}

static void on_loop(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_PRESSED) {
        if (!lv_btn_get_toggle(btn)) {
            // first press
            lv_btn_set_toggle(btn, true);
        }
        if (lv_btn_get_state(btn) == LV_BTN_STATE_TGL_PR) {
            ysw_sequencer_message_t message = {
                .type = YSW_SEQUENCER_LOOP,
                .loop.loop = false,
            };
            send_sequencer_message(&message);
        } else {
            ysw_sequencer_message_t message = {
                .type = YSW_SEQUENCER_LOOP,
                .loop.loop = true,
            };
            send_sequencer_message(&message);
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
        ysw_csf_set_cs(csf, cs);
    }
}

static void on_close(lv_obj_t * btn, lv_event_t event)
{
}

static void on_name_change(const char *new_name)
{
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
    ysw_cs_set_name(cs, new_name);
    ysw_csf_set_header_text(csf, new_name);
}

static void on_instrument_change(uint8_t new_instrument)
{
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
    ysw_cs_set_instrument(cs, new_instrument);
    stage();
}

static void on_octave_change(uint8_t new_octave)
{
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
    cs->octave = new_octave;
    stage();
}

static void on_mode_change(ysw_mode_t new_mode)
{
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
    cs->mode = new_mode;
    stage();
}

static void on_transposition_change(uint8_t new_transposition_index)
{
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
    cs->transposition = ysw_transposition_from_index(new_transposition_index);
    stage();
}

static void on_settings(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_RELEASED) {
        ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
        uint8_t index = ysw_transposition_to_index(cs->transposition);
        ysw_sdb_t *sdb = ysw_sdb_create("Chord Style Settings");
        ysw_sdb_add_string(sdb, on_name_change, "Name", cs->name);
        ysw_sdb_add_choice(sdb, on_instrument_change, "Instrument", cs->instrument, ysw_instruments);
        ysw_sdb_add_choice(sdb, on_octave_change, "Octave", cs->octave, ysw_octaves);
        ysw_sdb_add_choice(sdb, on_mode_change, "Mode", cs->mode, ysw_modes);
        ysw_sdb_add_choice(sdb, on_transposition_change, "Transposition", index, ysw_transposition);
    }
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
            if (csn_count) {
                refresh();
            }
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
            refresh();
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
        refresh();
    }
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

#if MODEL == 1
    ESP_LOGD(TAG, "main: configuring model 1");
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
    }
    eli_ili9341_xpt2046_initialize(&new_config);
#elif MODEL == 2
    ESP_LOGD(TAG, "main: configuring model 2");
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
#endif

    initialize_spiffs();
    initialize_synthesizer();
    initialize_sequencer();

    ysw_lv_styles_initialize();

    music = ysw_music_parse(MUSIC_DEFINITIONS);

    if (music) {
        ysw_csf_config_t config = {
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
        csf = ysw_csf_create(&config);
        ysw_cs_t *cs = ysw_music_get_cs(music, 0);
        ysw_csf_set_cs(csf, cs);
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
