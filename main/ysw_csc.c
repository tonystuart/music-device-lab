// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_csc.h"

#include "ysw_cs.h"
#include "ysw_csf.h"
#include "ysw_csn.h"
#include "ysw_instruments.h"
#include "ysw_lv_cse.h"
#include "ysw_main_sequencer.h"
#include "ysw_mode.h"
#include "ysw_music.h"
#include "ysw_name.h"
#include "ysw_octaves.h"
#include "ysw_sdb.h"
#include "ysw_sequencer.h"
#include "ysw_synthesizer.h"
#include "ysw_tempo.h"
#include "ysw_time.h"
#include "ysw_transposition.h"

#include "lvgl/lvgl.h"

#include "esp_log.h"

#define TAG "YSW_CSC"

typedef void (*note_visitor_t)(ysw_csn_t *csn);

static ysw_music_t *music;
static ysw_csf_t *csf;
static uint32_t cs_index;
static ysw_array_t *csn_clipboard;

static void stage()
{
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
    ysw_cs_sort_csn_array(cs);

    uint32_t note_count = 0;
    note_t *notes = ysw_cs_get_notes(cs, &note_count);

    ysw_sequencer_message_t message = {
        .type = YSW_SEQUENCER_STAGE,
        .stage.notes = notes,
        .stage.note_count = note_count,
        .stage.tempo = cs->tempo,
    };

    ysw_main_sequencer_send(&message);
}

static void pause()
{
    ysw_sequencer_message_t message = {
        .type = YSW_SEQUENCER_PAUSE,
    };

    ysw_main_sequencer_send(&message);
}

static void update_header()
{
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
    ysw_csf_set_header_text(csf, cs->name);
}

static void update_footer()
{
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d BPM %d/%d", cs->tempo,
            ysw_cs_get_beats_per_measure(cs), ysw_cs_get_beat_unit(cs));
    ysw_csf_set_footer_text(csf, buf);
}

static void update_frame()
{
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
    ysw_csf_set_cs(csf, cs);
    update_header();
    update_footer();
}

static void refresh()
{
    ysw_csf_redraw(csf);
    stage();
}

static void create_cs(uint32_t new_index)
{
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);

    char name[64];
    ysw_name_create(name, sizeof(name));
    ysw_cs_t *new_cs = ysw_cs_create(name,
            cs->instrument,
            cs->octave,
            cs->mode,
            cs->transposition,
            cs->tempo,
            cs->time);

    ysw_music_insert_cs(music, new_index, new_cs);
    cs_index = new_index;
    update_frame();
}

static void create_csn(lv_obj_t *cse, int8_t degree, uint8_t velocity, uint32_t start, uint32_t duration)
{
    ESP_LOGD(TAG, "create_csn degree=%d, velocity=%d, start=%d, duration=%d", degree, velocity, start, duration);
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
    uint16_t cs_duration = ysw_time_to_measure_duration(cs->time);
    if (duration > cs_duration) {
        duration = cs_duration;
        ESP_LOGD(TAG, "create_csn setting duration=%d", duration);
    }
    if (start + duration > cs_duration) {
        start = cs_duration - duration;
        ESP_LOGD(TAG, "create_csn setting start=%d", start);
    }

    ysw_csn_t *csn = ysw_csn_create(degree, velocity, start, duration, 0);
    ysw_cs_add_csn(cs, csn);
    refresh();
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
        update_frame();
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
            ysw_main_sequencer_send(&message);
        } else {
            ysw_sequencer_message_t message = {
                .type = YSW_SEQUENCER_LOOP,
                .loop.loop = true,
            };
            ysw_main_sequencer_send(&message);
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
        update_frame();
    }
}

static void on_close(lv_obj_t * btn, lv_event_t event)
{
}

static void on_name_change(const char *new_name)
{
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
    ysw_cs_set_name(cs, new_name);
    update_header();
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

static void on_tempo_change(uint8_t new_tempo_index)
{
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
    cs->tempo = ysw_tempo_from_index(new_tempo_index);
    update_footer();
    stage();
}

static void on_time_change(ysw_time_t new_time)
{
    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
    cs->time = new_time;
    update_footer();
    stage();
}

static void on_settings(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_RELEASED) {
        ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
        uint8_t trans_index = ysw_transposition_to_index(cs->transposition);
        uint8_t tempo_index = ysw_tempo_to_index(cs->tempo);
        ysw_sdb_t *sdb = ysw_sdb_create("Chord Style Settings");
        ysw_sdb_add_string(sdb, on_name_change, "Name", cs->name);
        ysw_sdb_add_choice(sdb, on_instrument_change, "Instrument", cs->instrument, ysw_instruments);
        ysw_sdb_add_choice(sdb, on_octave_change, "Octave", cs->octave, ysw_octaves);
        ysw_sdb_add_choice(sdb, on_mode_change, "Mode", cs->mode, ysw_modes);
        ysw_sdb_add_choice(sdb, on_transposition_change, "Transposition", trans_index, ysw_transposition);
        ysw_sdb_add_choice(sdb, on_tempo_change, "Tempo", tempo_index, ysw_tempo);
        ysw_sdb_add_choice(sdb, on_time_change, "Time", cs->time, ysw_time);
    }
}

static void on_save(lv_obj_t * btn, lv_event_t event)
{
}

static void on_new_chord_style(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_PRESSED) {
        create_cs(cs_index + 1);
    }
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

static void on_trash(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_PRESSED) {
        uint32_t target = 0;
        uint32_t changes = 0;
        ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);
        uint32_t csn_count = ysw_cs_get_csn_count(cs);
        for (uint32_t source = 0; source < csn_count; source++) {
            ysw_csn_t *csn = ysw_array_get(cs->csn_array, source);
            if (ysw_csn_is_selected(csn)) {
                ysw_csn_free(csn);
                changes++;
            } else {
                ysw_array_set(cs->csn_array, target, csn);
                target++;
            }
        }
        if (changes) {
            ysw_array_truncate(cs->csn_array, target);
            refresh();
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

static void cse_event_cb(lv_obj_t *cse, ysw_lv_cse_event_t event, ysw_lv_cse_event_cb_data_t *data)
{
    static uint8_t velocity = 80;
    static uint32_t duration = 80;

    switch (event) {
        case YSW_LV_CSE_SELECT:
            velocity = data->select.csn->velocity;
            duration = data->select.csn->duration;
            break;
        case YSW_LV_CSE_DESELECT:
            break;
        case YSW_LV_CSE_DRAG_END:
            stage();
            break;
        case YSW_LV_CSE_CREATE:
            create_csn(cse, data->create.degree, velocity, data->create.start, duration);
            break;
    }
}

void ysw_main_csc_initialize(ysw_music_t *new_music)
{
    music = new_music;

    ysw_csf_config_t config = {
        .next_cb = on_next,
        .play_cb = on_play,
        .pause_cb = on_pause,
        .loop_cb = on_loop,
        .prev_cb = on_prev,
        .close_cb = on_close,
        .settings_cb = on_settings,
        .save_cb = on_save,
        .new_cb = on_new_chord_style,
        .copy_cb = on_copy,
        .paste_cb = on_paste,
        .volume_mid_cb = on_volume_mid,
        .volume_max_cb = on_volume_max,
        .trash_cb = on_trash,
        .cse_event_cb = cse_event_cb,
    };

    csf = ysw_csf_create(&config);
    update_frame();
}

