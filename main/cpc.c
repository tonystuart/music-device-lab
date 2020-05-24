// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "cpc.h"

#include "sequencer.h"

#include "ysw_cp.h"
#include "ysw_cs.h"
#include "ysw_cpf.h"
#include "ysw_cn.h"
#include "ysw_instruments.h"
#include "ysw_lv_cpe.h"
#include "ysw_mode.h"
#include "ysw_music.h"
#include "ysw_name.h"
#include "ysw_octaves.h"
#include "ysw_sdb.h"
#include "ysw_sequencer.h"
#include "ysw_synthesizer.h"
#include "ysw_tempo.h"
#include "ysw_transposition.h"

#include "lvgl/lvgl.h"

#include "esp_log.h"

#define TAG "CPC"

typedef void (*step_visitor_t)(ysw_step_t *step);

static ysw_music_t *music;
static ysw_cpf_t *cpf;
static uint32_t cp_index;
static ysw_array_t *clipboard;

static void stage()
{
    ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);
    ysw_cp_sort_cn_array(cp);

    uint32_t note_count = 0;
    note_t *notes = ysw_cp_get_notes(cp, &note_count);

    ysw_sequencer_message_t message = {
        .type = YSW_SEQUENCER_STAGE,
        .stage.notes = notes,
        .stage.note_count = note_count,
        .stage.tempo = cp->tempo,
    };

    sequencer_send(&message);
}

static void pause()
{
    ysw_sequencer_message_t message = {
        .type = YSW_SEQUENCER_PAUSE,
    };

    sequencer_send(&message);
}

static void update_header()
{
    ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);
    ysw_cpf_set_header_text(cpf, cp->name);
}

static void update_footer()
{
    ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d BPM", cp->tempo);
    ysw_cpf_set_footer_text(cpf, buf);
}

static void update_frame()
{
    ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);
    ysw_cpf_set_cp(cpf, cp);
    update_header();
    update_footer();
}

static void refresh()
{
    ysw_cpf_redraw(cpf);
    stage();
}

static void create_cp(uint32_t new_index)
{
    ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);

    char name[64];
    ysw_name_create(name, sizeof(name));
    ysw_cp_t *new_cp = ysw_cp_create(name,
            cp->instrument,
            cp->octave,
            cp->mode,
            cp->transposition,
            cp->tempo);

    ysw_music_insert_cp(music, new_index, new_cp);
    cp_index = new_index;
    update_frame();
}

static void create_step(lv_obj_t *cpe, ysw_lv_cpe_create_t *create)
{
    ESP_LOGD(TAG, "create_step step_index=%d, degree=%d", create->step_index, create->degree);
    if (ysw_music_get_cs_count(music)) {
        ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);
        ysw_cs_t *cs = ysw_music_get_cs(music, 0);
        ysw_step_t *step = ysw_step_create(cs, create->degree, YSW_STEP_NEW_MEASURE);
        uint32_t index = create->step_index;
        uint32_t step_count = ysw_cp_get_step_count(cp);
        if (index > step_count) {
            index = step_count;
        }
        ESP_LOGD(TAG, "create_step cp_index=%d, step_index=%d", cp_index, index);
        ysw_cp_insert_step(cp, index, step);
        refresh();
    }
}

static void copy_to_clipboard(ysw_step_t *step)
{
    ysw_step_t *new_step = ysw_step_copy(step);
    ysw_step_select(step, false);
    ysw_step_select(new_step, true);
    ysw_array_push(clipboard, new_step);
}

static void visit_steps(step_visitor_t visitor, lv_event_t event)
{
    if (event == LV_EVENT_PRESSED || event == LV_EVENT_LONG_PRESSED_REPEAT) {
        uint32_t visitor_count = 0;
        ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);
        uint32_t step_count = ysw_cp_get_step_count(cp);
        for (uint32_t i = 0; i < step_count; i++) {
            ysw_step_t *step = ysw_cp_get_step(cp, i);
            if (ysw_step_is_selected(step)) {
                visitor(step);
                visitor_count++;
            }
        }
        if (visitor_count) {
            ysw_cpf_redraw(cpf);
        }
    } else if (event == LV_EVENT_RELEASED) {
        stage();
    }
}

static void on_next(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_RELEASED) {
        uint32_t cp_count = ysw_music_get_cp_count(music);
        if (++cp_index >= cp_count) {
            cp_index = 0;
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
            sequencer_send(&message);
        } else {
            ysw_sequencer_message_t message = {
                .type = YSW_SEQUENCER_LOOP,
                .loop.loop = true,
            };
            sequencer_send(&message);
        }
    }
}

static void on_prev(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_RELEASED) {
        uint32_t cp_count = ysw_music_get_cp_count(music);
        if (--cp_index >= cp_count) {
            cp_index = cp_count - 1;
        }
        update_frame();
    }
}

static void on_close(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_RELEASED) {
        ysw_cpf_del(cpf);
    }
}

static void on_name_change(const char *new_name)
{
    ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);
    ysw_cp_set_name(cp, new_name);
    update_header();
}

static void on_instrument_change(uint8_t new_instrument)
{
    ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);
    ysw_cp_set_instrument(cp, new_instrument);
    stage();
}

static void on_octave_change(uint8_t new_octave)
{
    ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);
    cp->octave = new_octave;
    stage();
}

static void on_mode_change(ysw_mode_t new_mode)
{
    ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);
    cp->mode = new_mode;
    stage();
}

static void on_transposition_change(uint8_t new_transposition_index)
{
    ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);
    cp->transposition = ysw_transposition_from_index(new_transposition_index);
    stage();
}

static void on_tempo_change(uint8_t new_tempo_index)
{
    ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);
    cp->tempo = ysw_tempo_from_index(new_tempo_index);
    update_footer();
    stage();
}

static void on_settings(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_RELEASED) {
        ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);
        uint8_t trans_index = ysw_transposition_to_index(cp->transposition);
        uint8_t tempo_index = ysw_tempo_to_index(cp->tempo);
        ysw_sdb_t *sdb = ysw_sdb_create("Chord Progression Settings");
        ysw_sdb_add_string(sdb, on_name_change, "Name", cp->name);
        ysw_sdb_add_choice(sdb, on_instrument_change, "Instrument", cp->instrument, ysw_instruments);
        ysw_sdb_add_choice(sdb, on_octave_change, "Octave", cp->octave, ysw_octaves);
        ysw_sdb_add_choice(sdb, on_mode_change, "Mode", cp->mode, ysw_modes);
        ysw_sdb_add_choice(sdb, on_transposition_change, "Transposition", trans_index, ysw_transposition);
        ysw_sdb_add_choice(sdb, on_tempo_change, "Tempo", tempo_index, ysw_tempo);
    }
}

static void on_save(lv_obj_t * btn, lv_event_t event)
{
}

static void on_new_chord_style(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_PRESSED) {
        create_cp(cp_index + 1);
    }
}

static void on_copy(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_PRESSED) {
        if (clipboard) {
            uint32_t old_step_count = ysw_array_get_count(clipboard);
            for (int i = 0; i < old_step_count; i++) {
                ysw_step_t *old_step = ysw_array_get(clipboard, i);
                ysw_step_free(old_step);
            }
            ysw_array_truncate(clipboard, 0);
        } else {
            clipboard = ysw_array_create(10);
        }
        visit_steps(copy_to_clipboard, event);
    }
}

static void on_paste(lv_obj_t * btn, lv_event_t event)
{
    if (event == LV_EVENT_PRESSED) {
        if (clipboard) {
            ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);
            uint32_t step_count = ysw_array_get_count(clipboard);
            for (uint32_t i = 0; i < step_count; i++) {
                ysw_step_t *step = ysw_array_get(clipboard, i);
                ysw_step_t *new_step = ysw_step_copy(step);
                new_step->state = step->state;
                ysw_cp_add_step(cp, new_step);
            }
            if (step_count) {
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
        ysw_cp_t *cp = ysw_music_get_cp(music, cp_index);
        uint32_t step_count = ysw_cp_get_step_count(cp);
        for (uint32_t source = 0; source < step_count; source++) {
            ysw_step_t *step = ysw_array_get(cp->steps, source);
            if (ysw_step_is_selected(step)) {
                ysw_step_free(step);
                changes++;
            } else {
                ysw_array_set(cp->steps, target, step);
                target++;
            }
        }
        if (changes) {
            ysw_array_truncate(cp->steps, target);
            refresh();
        }
    }
}

static void on_left(lv_obj_t * btn, lv_event_t event)
{
}

static void on_right(lv_obj_t * btn, lv_event_t event)
{
}

static void cpe_event_cb(lv_obj_t *cpe, ysw_lv_cpe_event_t event, ysw_lv_cpe_event_cb_data_t *data)
{
    switch (event) {
        case YSW_LV_CPE_SELECT:
            break;
        case YSW_LV_CPE_DESELECT:
            break;
        case YSW_LV_CPE_DRAG_END:
            stage();
            break;
        case YSW_LV_CPE_CREATE:
            create_step(cpe, &data->create);
            break;
    }
}

void cpc_create(ysw_music_t *new_music, uint32_t new_cp_index)
{
    music = new_music;
    cp_index = new_cp_index;

    ysw_cpf_config_t config = {
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
        .left_cb = on_left,
        .right_cb = on_right,
        .trash_cb = on_trash,
        .cpe_event_cb = cpe_event_cb,
    };

    cpf = ysw_cpf_create(&config);
    update_frame();
}

void cpc_on_metro(note_t *metro_note)
{
    if (cpf) {
        ysw_cpf_on_metro(cpf, metro_note);
    }
}

