// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "hpc.h"

#include "sequencer.h"

#include "ysw_hp.h"
#include "ysw_cs.h"
#include "ysw_sn.h"
#include "ysw_heap.h"
#include "ysw_instruments.h"
#include "ysw_lv_hpe.h"
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

#define TAG "HPC"

typedef void (*step_visitor_t)(hpc_t *hpc, ysw_step_t *step);

static void send_notes(hpc_t *hpc, ysw_sequencer_message_type_t type)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    ysw_hp_sort_sn_array(hp);

    uint32_t note_count = 0;
    ysw_note_t *notes = ysw_hp_get_notes(hp, &note_count);

    ysw_sequencer_message_t message = {
        .type = type,
        .stage.notes = notes,
        .stage.note_count = note_count,
        .stage.tempo = hp->tempo,
    };

    sequencer_send(&message);
}

static void play(hpc_t *hpc)
{
    send_notes(hpc, YSW_SEQUENCER_PLAY);
}

static void stage(hpc_t *hpc)
{
    if (ysw_lv_hpe_gs.auto_play) {
        send_notes(hpc, YSW_SEQUENCER_STAGE);
    }
}

static void stop()
{
    ysw_sequencer_message_t message = {
        .type = YSW_SEQUENCER_PAUSE,
    };

    sequencer_send(&message);
}

static void update_header(hpc_t *hpc)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    ysw_frame_set_header_text(hpc->frame, hp->name);
}

static void update_footer(hpc_t *hpc)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d BPM", hp->tempo);
    ysw_frame_set_footer_text(hpc->frame, buf);
}

static void update_frame(hpc_t *hpc)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    ysw_lv_hpe_set_hp(hpc->hpe, hp);
    update_header(hpc);
    update_footer(hpc);
}

static void refresh(hpc_t *hpc)
{
    lv_obj_invalidate(hpc->hpe);
    stage(hpc);
}

static void create_hp(hpc_t *hpc, uint32_t new_index)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);

    char name[64];
    ysw_name_create(name, sizeof(name));
    ysw_hp_t *new_hp = ysw_hp_create(name,
            hp->instrument,
            hp->octave,
            hp->mode,
            hp->transposition,
            hp->tempo);

    ysw_music_insert_hp(hpc->music, new_index, new_hp);
    hpc->hp_index = new_index;
    update_frame(hpc);
}

static void copy_to_clipboard(hpc_t *hpc, ysw_step_t *step)
{
    ysw_step_t *new_step = ysw_step_copy(step);
    ysw_step_select(step, false);
    ysw_step_select(new_step, true);
    ysw_array_push(hpc->clipboard, new_step);
}

static void visit_steps(hpc_t *hpc, step_visitor_t visitor)
{
    uint32_t visitor_count = 0;
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    uint32_t step_count = ysw_hp_get_step_count(hp);
    for (uint32_t i = 0; i < step_count; i++) {
        ysw_step_t *step = ysw_hp_get_step(hp, i);
        if (ysw_step_is_selected(step)) {
            visitor(hpc, step);
            visitor_count++;
        }
    }
    if (visitor_count) {
        lv_obj_invalidate(hpc->hpe);
    }
}

static void on_name_change(hpc_t *hpc, const char *new_name)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    ysw_hp_set_name(hp, new_name);
    update_header(hpc);
}

static void on_instrument_change(hpc_t *hpc, uint8_t new_instrument)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    ysw_hp_set_instrument(hp, new_instrument);
    stage(hpc);
}

static void on_octave_change(hpc_t *hpc, uint8_t new_octave)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    hp->octave = new_octave;
    stage(hpc);
}

static void on_mode_change(hpc_t *hpc, ysw_mode_t new_mode)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    hp->mode = new_mode;
    stage(hpc);
}

static void on_transposition_change(hpc_t *hpc, uint8_t new_transposition_index)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    hp->transposition = ysw_transposition_from_index(new_transposition_index);
    stage(hpc);
}

static void on_tempo_change(hpc_t *hpc, uint8_t new_tempo_index)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    hp->tempo = ysw_tempo_from_index(new_tempo_index);
    update_footer(hpc);
    stage(hpc);
}

static void on_next(hpc_t *hpc, lv_obj_t *btn)
{
    uint32_t hp_count = ysw_music_get_hp_count(hpc->music);
    if (++(hpc->hp_index) >= hp_count) {
        hpc->hp_index = 0;
    }
    update_frame(hpc);
}

static void on_play(hpc_t *hpc, lv_obj_t *btn)
{
    play(hpc);
}

static void on_stop(hpc_t *hpc, lv_obj_t *btn)
{
    stop();
}

static void on_loop(hpc_t *hpc, lv_obj_t *btn)
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

static void on_prev(hpc_t *hpc, lv_obj_t *btn)
{
    uint32_t hp_count = ysw_music_get_hp_count(hpc->music);
    if (--(hpc->hp_index) >= hp_count) {
        hpc->hp_index = hp_count - 1;
    }
    update_frame(hpc);
}

static void on_close(hpc_t *hpc, lv_obj_t *btn)
{
    if (hpc->close_cb) {
        hpc->close_cb(hpc->close_cb_context, hpc);
    }
    ysw_frame_del(hpc->frame); // deletes contents
    ysw_heap_free(hpc);
}

static void on_settings(hpc_t *hpc, lv_obj_t *btn)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    uint8_t trans_index = ysw_transposition_to_index(hp->transposition);
    uint8_t tempo_index = ysw_tempo_to_index(hp->tempo);
    ysw_sdb_t *sdb = ysw_sdb_create("Chord Progression Settings", hpc);
    ysw_sdb_add_string(sdb, "Name", hp->name, on_name_change);
    ysw_sdb_add_choice(sdb, "Instrument", hp->instrument, ysw_instruments, on_instrument_change);
    ysw_sdb_add_choice(sdb, "Octave", hp->octave, ysw_octaves, on_octave_change);
    ysw_sdb_add_choice(sdb, "Mode", hp->mode, ysw_modes, on_mode_change);
    ysw_sdb_add_choice(sdb, "Transposition", trans_index, ysw_transposition, on_transposition_change);
    ysw_sdb_add_choice(sdb, "Tempo", tempo_index, ysw_tempo, on_tempo_change);
}

static void on_save(hpc_t *hpc, lv_obj_t *btn)
{
}

static void on_new(hpc_t *hpc, lv_obj_t *btn)
{
    create_hp(hpc, hpc->hp_index + 1);
}

static void on_copy(hpc_t *hpc, lv_obj_t *btn)
{
    if (hpc->clipboard) {
        uint32_t old_step_count = ysw_array_get_count(hpc->clipboard);
        for (int i = 0; i < old_step_count; i++) {
            ysw_step_t *old_step = ysw_array_get(hpc->clipboard, i);
            ysw_step_free(old_step);
        }
        ysw_array_truncate(hpc->clipboard, 0);
    } else {
        hpc->clipboard = ysw_array_create(10);
    }
    visit_steps(hpc, copy_to_clipboard);
}

static void on_paste(hpc_t *hpc, lv_obj_t *btn)
{
    ESP_LOGD(TAG, "on_paste entered");
    if (hpc->clipboard) {
        uint32_t paste_count = ysw_array_get_count(hpc->clipboard);
        if (paste_count) {
            ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
            uint32_t step_count = ysw_hp_get_step_count(hp);
            for (uint32_t i = 0; i < step_count; i++) {
                ysw_step_t *step = ysw_hp_get_step(hp, i);
                ysw_step_select(step, false);
            }
            uint32_t insert_index;
            if (hpc->step_index < step_count) {
                // paste after most recently selected step
                insert_index = hpc->step_index + 1;
            } else {
                // paste at end (which is also beginning, if empty)
                insert_index = step_count;
            }
            uint32_t first = insert_index;
            ESP_LOGD(TAG, "step_index=%d, paste_count=%d, step_count=%d, insert_index=%d", hpc->step_index, paste_count, step_count, insert_index);
            for (uint32_t i = 0; i < paste_count; i++) {
                ysw_step_t *step = ysw_array_get(hpc->clipboard, i);
                ysw_step_t *new_step = ysw_step_copy(step);
                new_step->state = step->state;
                ysw_hp_insert_step(hp, insert_index, new_step);
                hpc->step_index = ++insert_index;
            }
            ysw_lv_hpe_ensure_visible(hpc->hpe, first, first + paste_count - 1); // -1 because width is included
            ESP_LOGD(TAG, "on_paste calling refresh");
            refresh(hpc);
        }
    }
}

static void on_trash(hpc_t *hpc, lv_obj_t *btn)
{
    uint32_t target = 0;
    uint32_t changes = 0;
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    uint32_t step_count = ysw_hp_get_step_count(hp);
    for (uint32_t source = 0; source < step_count; source++) {
        ysw_step_t *step = ysw_hp_get_step(hp, source);
        if (ysw_step_is_selected(step)) {
            ysw_step_free(step);
            changes++;
        } else {
            ysw_array_set(hp->steps, target, step);
            target++;
        }
    }
    if (changes) {
        ysw_array_truncate(hp->steps, target);
        refresh(hpc);
    }
}

static void on_left(hpc_t *hpc, lv_obj_t *btn)
{
    uint32_t changes = 0;
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    uint32_t step_count = ysw_hp_get_step_count(hp);
    for (int32_t i = 0, j = 1; j < step_count; i++, j++) {
        ysw_step_t *hpc_step = ysw_hp_get_step(hp, j);
        if (ysw_step_is_selected(hpc_step)) {
            ysw_step_t *other_step = ysw_array_get(hp->steps, i);
            ysw_array_set(hp->steps, i, hpc_step);
            ysw_array_set(hp->steps, j, other_step);
            changes++;
        }
    }
    if (changes) {
        refresh(hpc);
    }
}

static void on_right(hpc_t *hpc, lv_obj_t *btn)
{
    uint32_t changes = 0;
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    uint32_t step_count = ysw_hp_get_step_count(hp);
    for (int32_t i = step_count - 1, j = step_count - 2; j >= 0; i--, j--) {
        ysw_step_t *hpc_step = ysw_hp_get_step(hp, j);
        if (ysw_step_is_selected(hpc_step)) {
            ysw_step_t *other_step = ysw_array_get(hp->steps, i);
            ysw_array_set(hp->steps, i, hpc_step);
            ysw_array_set(hp->steps, j, other_step);
            changes++;
        }
    }
    if (changes) {
        refresh(hpc);
    }
}

static void on_create_step(hpc_t *hpc, uint32_t step_index, uint8_t degree)
{
    if (ysw_music_get_cs_count(hpc->music)) {
        ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
        uint32_t step_count = ysw_hp_get_step_count(hp);
        ysw_cs_t *template_cs;
        if (hpc->step_index < step_count) {
            ysw_step_t *last_step = ysw_hp_get_step(hp, hpc->step_index);
            template_cs = last_step->cs;
        } else {
            template_cs = ysw_music_get_cs(hpc->music, 0);
        }
        ysw_step_t *step = ysw_step_create(template_cs, degree, YSW_STEP_NEW_MEASURE);
        step_count++;
        hpc->step_index = step_index;
        if (hpc->step_index > step_count) {
            hpc->step_index = step_count;
        }
        ysw_hp_insert_step(hp, hpc->step_index, step);
        ysw_step_select(step, true);
        refresh(hpc);
    }
}

static void on_edit_step(hpc_t *hpc, ysw_step_t *step)
{
    ysw_step_select(step, true);
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    hpc->step_index = ysw_hp_get_step_index(hp, step);
    ysw_ssc_create(hpc->music, hp, hpc->step_index);
}

static void on_select(hpc_t *hpc, ysw_step_t *step)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    hpc->step_index = ysw_hp_get_step_index(hp, step);
    ESP_LOGD(TAG, "on_select step_index=%d", hpc->step_index);
}

static ysw_frame_t *create_frame(hpc_t *hpc)
{
    ysw_frame_t *frame = ysw_frame_create(hpc);

    ysw_frame_add_footer_button(frame, LV_SYMBOL_SETTINGS, on_settings);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_SAVE, on_save);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_AUDIO, on_new);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_COPY, on_copy);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_PASTE, on_paste);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_TRASH, on_trash);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_LEFT, on_left);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_RIGHT, on_right);

    ysw_frame_add_header_button(frame, LV_SYMBOL_CLOSE, on_close);
    ysw_frame_add_header_button(frame, LV_SYMBOL_NEXT, on_next);
    ysw_frame_add_header_button(frame, LV_SYMBOL_LOOP, on_loop);
    ysw_frame_add_header_button(frame, LV_SYMBOL_STOP, on_stop);
    ysw_frame_add_header_button(frame, LV_SYMBOL_PLAY, on_play);
    ysw_frame_add_header_button(frame, LV_SYMBOL_PREV, on_prev);

    return frame;
}

hpc_t *hpc_create(ysw_music_t *music, uint32_t hp_index)
{
    hpc_t *hpc = ysw_heap_allocate(sizeof(hpc_t)); // freed in on_close
    hpc->music = music;
    hpc->hp_index = hp_index;
    hpc->step_index = 0;
    hpc->frame = create_frame(hpc);
    hpc->hpe = ysw_lv_hpe_create(hpc->frame->win, hpc);
    ysw_lv_hpe_set_create_cb(hpc->hpe, on_create_step);
    ysw_lv_hpe_set_edit_cb(hpc->hpe, on_edit_step);
    ysw_lv_hpe_set_select_cb(hpc->hpe, on_select);
    ysw_lv_hpe_set_drag_end_cb(hpc->hpe, stage);
    ysw_frame_set_content(hpc->frame, hpc->hpe);
    update_frame(hpc);
    return hpc;
}

void hpc_set_close_cb(hpc_t *hpc, hpc_close_cb_t cb, void *context)
{
    hpc->close_cb = cb;
    hpc->close_cb_context = context;
}

void hpc_on_metro(hpc_t *hpc, ysw_note_t *metro_note)
{
    ysw_lv_hpe_on_metro(hpc->hpe, metro_note);
}

