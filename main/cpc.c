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
#include "ysw_cn.h"
#include "ysw_heap.h"
#include "ysw_instruments.h"
#include "ysw_lv_cpe.h"
#include "ysw_mode.h"
#include "ysw_music.h"
#include "ysw_name.h"
#include "ysw_octaves.h"
#include "ysw_sdb.h"
#include "ysw_sequencer.h"
#include "ysw_ssc.h"
#include "ysw_synthesizer.h"
#include "ysw_tempo.h"
#include "ysw_transposition.h"

#include "lvgl/lvgl.h"

#include "esp_log.h"

#define TAG "CPC"

typedef void (*step_visitor_t)(cpc_t *cpc, ysw_step_t *step);

static void send_notes(cpc_t *cpc, ysw_sequencer_message_type_t type)
{
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    ysw_cp_sort_cn_array(cp);

    uint32_t note_count = 0;
    note_t *notes = ysw_cp_get_notes(cp, &note_count);

    ysw_sequencer_message_t message = {
        .type = type,
        .stage.notes = notes,
        .stage.note_count = note_count,
        .stage.tempo = cp->tempo,
    };

    sequencer_send(&message);
}

static void play(cpc_t *cpc)
{
    send_notes(cpc, YSW_SEQUENCER_PLAY);
}

static void stage(cpc_t *cpc)
{
    if (ysw_lv_cpe_gs.auto_play) {
        send_notes(cpc, YSW_SEQUENCER_STAGE);
    }
}

static void stop()
{
    ysw_sequencer_message_t message = {
        .type = YSW_SEQUENCER_PAUSE,
    };

    sequencer_send(&message);
}

static void update_header(cpc_t *cpc)
{
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    ysw_frame_set_header_text(cpc->frame, cp->name);
}

static void update_footer(cpc_t *cpc)
{
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d BPM", cp->tempo);
    ysw_frame_set_footer_text(cpc->frame, buf);
}

static void update_frame(cpc_t *cpc)
{
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    ysw_lv_cpe_set_cp(cpc->cpe, cp);
    update_header(cpc);
    update_footer(cpc);
}

static void refresh(cpc_t *cpc)
{
    lv_obj_invalidate(cpc->cpe);
    stage(cpc);
}

static void create_cp(cpc_t *cpc, uint32_t new_index)
{
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);

    char name[64];
    ysw_name_create(name, sizeof(name));
    ysw_cp_t *new_cp = ysw_cp_create(name,
            cp->instrument,
            cp->octave,
            cp->mode,
            cp->transposition,
            cp->tempo);

    ysw_music_insert_cp(cpc->music, new_index, new_cp);
    cpc->cp_index = new_index;
    update_frame(cpc);
}

static void copy_to_clipboard(cpc_t *cpc, ysw_step_t *step)
{
    ysw_step_t *new_step = ysw_step_copy(step);
    ysw_step_select(step, false);
    ysw_step_select(new_step, true);
    ysw_array_push(cpc->clipboard, new_step);
}

static void visit_steps(cpc_t *cpc, step_visitor_t visitor)
{
    uint32_t visitor_count = 0;
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    uint32_t step_count = ysw_cp_get_step_count(cp);
    for (uint32_t i = 0; i < step_count; i++) {
        ysw_step_t *step = ysw_cp_get_step(cp, i);
        if (ysw_step_is_selected(step)) {
            visitor(cpc, step);
            visitor_count++;
        }
    }
    if (visitor_count) {
        lv_obj_invalidate(cpc->cpe);
    }
}

static void on_name_change(cpc_t *cpc, const char *new_name)
{
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    ysw_cp_set_name(cp, new_name);
    update_header(cpc);
}

static void on_instrument_change(cpc_t *cpc, uint8_t new_instrument)
{
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    ysw_cp_set_instrument(cp, new_instrument);
    stage(cpc);
}

static void on_octave_change(cpc_t *cpc, uint8_t new_octave)
{
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    cp->octave = new_octave;
    stage(cpc);
}

static void on_mode_change(cpc_t *cpc, ysw_mode_t new_mode)
{
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    cp->mode = new_mode;
    stage(cpc);
}

static void on_transposition_change(cpc_t *cpc, uint8_t new_transposition_index)
{
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    cp->transposition = ysw_transposition_from_index(new_transposition_index);
    stage(cpc);
}

static void on_tempo_change(cpc_t *cpc, uint8_t new_tempo_index)
{
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    cp->tempo = ysw_tempo_from_index(new_tempo_index);
    update_footer(cpc);
    stage(cpc);
}

static void on_next(cpc_t *cpc, lv_obj_t *btn)
{
    uint32_t cp_count = ysw_music_get_cp_count(cpc->music);
    if (++(cpc->cp_index) >= cp_count) {
        cpc->cp_index = 0;
    }
    update_frame(cpc);
}

static void on_play(cpc_t *cpc, lv_obj_t *btn)
{
    play(cpc);
}

static void on_stop(cpc_t *cpc, lv_obj_t *btn)
{
    stop();
}

static void on_loop(cpc_t *cpc, lv_obj_t *btn)
{
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

static void on_prev(cpc_t *cpc, lv_obj_t *btn)
{
    uint32_t cp_count = ysw_music_get_cp_count(cpc->music);
    if (--(cpc->cp_index) >= cp_count) {
        cpc->cp_index = cp_count - 1;
    }
    update_frame(cpc);
}

static void on_close(cpc_t *cpc, lv_obj_t *btn)
{
    if (cpc->close_cb) {
        cpc->close_cb(cpc->close_cb_context, cpc);
    }
    ysw_frame_del(cpc->frame); // deletes contents
    ysw_heap_free(cpc);
}

static void on_settings(cpc_t *cpc, lv_obj_t *btn)
{
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    uint8_t trans_index = ysw_transposition_to_index(cp->transposition);
    uint8_t tempo_index = ysw_tempo_to_index(cp->tempo);
    ysw_sdb_t *sdb = ysw_sdb_create("Chord Progression Settings", cpc);
    ysw_sdb_add_string(sdb, "Name", cp->name, on_name_change);
    ysw_sdb_add_choice(sdb, "Instrument", cp->instrument, ysw_instruments, on_instrument_change);
    ysw_sdb_add_choice(sdb, "Octave", cp->octave, ysw_octaves, on_octave_change);
    ysw_sdb_add_choice(sdb, "Mode", cp->mode, ysw_modes, on_mode_change);
    ysw_sdb_add_choice(sdb, "Transposition", trans_index, ysw_transposition, on_transposition_change);
    ysw_sdb_add_choice(sdb, "Tempo", tempo_index, ysw_tempo, on_tempo_change);
}

static void on_save(cpc_t *cpc, lv_obj_t *btn)
{
}

static void on_new(cpc_t *cpc, lv_obj_t *btn)
{
    create_cp(cpc, cpc->cp_index + 1);
}

static void on_copy(cpc_t *cpc, lv_obj_t *btn)
{
    if (cpc->clipboard) {
        uint32_t old_step_count = ysw_array_get_count(cpc->clipboard);
        for (int i = 0; i < old_step_count; i++) {
            ysw_step_t *old_step = ysw_array_get(cpc->clipboard, i);
            ysw_step_free(old_step);
        }
        ysw_array_truncate(cpc->clipboard, 0);
    } else {
        cpc->clipboard = ysw_array_create(10);
    }
    visit_steps(cpc, copy_to_clipboard);
}

static void on_paste(cpc_t *cpc, lv_obj_t *btn)
{
    ESP_LOGD(TAG, "on_paste entered");
    if (cpc->clipboard) {
        uint32_t paste_count = ysw_array_get_count(cpc->clipboard);
        if (paste_count) {
            ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
            uint32_t step_count = ysw_cp_get_step_count(cp);
            for (uint32_t i = 0; i < step_count; i++) {
                ysw_step_t *step = ysw_cp_get_step(cp, i);
                ysw_step_select(step, false);
            }
            uint32_t insert_index;
            if (cpc->step_index < 0) {
                insert_index = 0;
            } else if (cpc->step_index < step_count) {
                insert_index = cpc->step_index + 1;
            } else {
                insert_index = step_count;
            }
            uint32_t first = insert_index;
            ESP_LOGD(TAG, "step_index=%d, paste_count=%d, step_count=%d, insert_index=%d", cpc->step_index, paste_count, step_count, insert_index);
            for (uint32_t i = 0; i < paste_count; i++) {
                ysw_step_t *step = ysw_array_get(cpc->clipboard, i);
                ysw_step_t *new_step = ysw_step_copy(step);
                new_step->state = step->state;
                ysw_cp_insert_step(cp, insert_index, new_step);
                cpc->step_index = ++insert_index;
            }
            ysw_lv_cpe_ensure_visible(cpc->cpe, first, first + paste_count - 1); // -1 because width is included
            ESP_LOGD(TAG, "on_paste calling refresh");
            refresh(cpc);
        }
    }
}

static void on_trash(cpc_t *cpc, lv_obj_t *btn)
{
    uint32_t target = 0;
    uint32_t changes = 0;
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    uint32_t step_count = ysw_cp_get_step_count(cp);
    for (uint32_t source = 0; source < step_count; source++) {
        ysw_step_t *step = ysw_cp_get_step(cp, source);
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
        refresh(cpc);
    }
}

static void on_left(cpc_t *cpc, lv_obj_t *btn)
{
    uint32_t changes = 0;
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    uint32_t step_count = ysw_cp_get_step_count(cp);
    for (int32_t i = 0, j = 1; j < step_count; i++, j++) {
        ysw_step_t *cpc_step = ysw_cp_get_step(cp, j);
        if (ysw_step_is_selected(cpc_step)) {
            ysw_step_t *other_step = ysw_array_get(cp->steps, i);
            ysw_array_set(cp->steps, i, cpc_step);
            ysw_array_set(cp->steps, j, other_step);
            changes++;
        }
    }
    if (changes) {
        refresh(cpc);
    }
}

static void on_right(cpc_t *cpc, lv_obj_t *btn)
{
    uint32_t changes = 0;
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    uint32_t step_count = ysw_cp_get_step_count(cp);
    for (int32_t i = step_count - 1, j = step_count - 2; j >= 0; i--, j--) {
        ysw_step_t *cpc_step = ysw_cp_get_step(cp, j);
        if (ysw_step_is_selected(cpc_step)) {
            ysw_step_t *other_step = ysw_array_get(cp->steps, i);
            ysw_array_set(cp->steps, i, cpc_step);
            ysw_array_set(cp->steps, j, other_step);
            changes++;
        }
    }
    if (changes) {
        refresh(cpc);
    }
}

static void on_edit(cpc_t *cpc, lv_obj_t *btn)
{
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    uint32_t step_count = ysw_cp_get_step_count(cp);
    if (step_count) {
        if (cpc->step_index < 0) {
            cpc->step_index = 0;
        } else if (cpc->step_index >= step_count) {
            cpc->step_index = step_count - 1;
        }
        ysw_step_t *step = ysw_cp_get_step(cp, cpc->step_index);
        ysw_step_select(step, true);
        ysw_ssc_create(cpc->music, cp, cpc->step_index);
    }
}

static void on_create_step(cpc_t *cpc, uint32_t step_index, uint8_t degree)
{
    ESP_LOGD(TAG, "create_step step_index=%d, degree=%d", step_index, degree);
    if (ysw_music_get_cs_count(cpc->music)) {
        ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
        ysw_cs_t *cs = ysw_music_get_cs(cpc->music, 0);
        ysw_step_t *step = ysw_step_create(cs, degree, YSW_STEP_NEW_MEASURE);
        uint32_t index = step_index;
        uint32_t step_count = ysw_cp_get_step_count(cp);
        if (index > step_count) {
            index = step_count;
        }
        ESP_LOGD(TAG, "create_step cp_index=%d, step_index=%d", cpc->cp_index, index);
        ysw_cp_insert_step(cp, index, step);
        refresh(cpc);
    }
}

static void on_select(cpc_t *cpc, ysw_step_t *step)
{
    ysw_cp_t *cp = ysw_music_get_cp(cpc->music, cpc->cp_index);
    cpc->step_index = ysw_cp_get_step_index(cp, step);
    ESP_LOGD(TAG, "on_select step_index=%d", cpc->step_index);
}

static ysw_frame_t *create_frame(cpc_t *cpc)
{
    ysw_frame_t *frame = ysw_frame_create(cpc);

    ysw_frame_add_footer_button(frame, LV_SYMBOL_SETTINGS, on_settings);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_SAVE, on_save);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_AUDIO, on_new);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_COPY, on_copy);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_PASTE, on_paste);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_TRASH, on_trash);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_LEFT, on_left);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_RIGHT, on_right);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_EDIT, on_edit);

    ysw_frame_add_header_button(frame, LV_SYMBOL_CLOSE, on_close);
    ysw_frame_add_header_button(frame, LV_SYMBOL_NEXT, on_next);
    ysw_frame_add_header_button(frame, LV_SYMBOL_LOOP, on_loop);
    ysw_frame_add_header_button(frame, LV_SYMBOL_STOP, on_stop);
    ysw_frame_add_header_button(frame, LV_SYMBOL_PLAY, on_play);
    ysw_frame_add_header_button(frame, LV_SYMBOL_PREV, on_prev);

    return frame;
}

cpc_t *cpc_create(ysw_music_t *music, uint32_t cp_index)
{
    cpc_t *cpc = ysw_heap_allocate(sizeof(cpc_t)); // freed in on_close
    cpc->music = music;
    cpc->cp_index = cp_index;
    cpc->step_index = -1;
    cpc->frame = create_frame(cpc);
    cpc->cpe = ysw_lv_cpe_create(cpc->frame->win, cpc);
    ysw_lv_cpe_set_create_cb(cpc->cpe, on_create_step);
    ysw_lv_cpe_set_select_cb(cpc->cpe, on_select);
    ysw_lv_cpe_set_drag_end_cb(cpc->cpe, stage);
    ysw_frame_set_content(cpc->frame, cpc->cpe);
    update_frame(cpc);
    return cpc;
}

void cpc_set_close_cb(cpc_t *cpc, cpc_close_cb_t cb, void *context)
{
    cpc->close_cb = cb;
    cpc->close_cb_context = context;
}

void cpc_on_metro(cpc_t *cpc, note_t *metro_note)
{
    ysw_lv_cpe_on_metro(cpc->cpe, metro_note);
}

