// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "hpc.h"

#include "seq.h"

#include "ysw_auto_play.h"
#include "ysw_hp.h"
#include "ysw_cs.h"
#include "ysw_sn.h"
#include "ysw_heap.h"
#include "ysw_instruments.h"
#include "ysw_lv_hpe.h"
#include "ysw_mfw.h"
#include "ysw_mode.h"
#include "ysw_music.h"
#include "ysw_name.h"
#include "ysw_octaves.h"
#include "ysw_sdb.h"
#include "ysw_seq.h"
#include "ysw_ssc.h"
#include "ysw_synthesizer.h"
#include "ysw_tempo.h"
#include "ysw_transposition.h"

#include "lvgl/lvgl.h"

#include "esp_log.h"

#define TAG "HPC"

typedef void (*ps_visitor_t)(hpc_t *hpc, ysw_ps_t *ps);

// NB: invoked by sequencer task
static void on_sequencer_status(hpc_t *hpc, ysw_seq_status_message_t *message)
{
    if (message->type == YSW_SEQ_IDLE) {
        ysw_lv_hpe_on_metro(hpc->hpe, NULL);
    } else if (message->type == YSW_SEQ_NOTE) {
        ysw_lv_hpe_on_metro(hpc->hpe, message->note);
    }
}

static void send_notes(hpc_t *hpc, ysw_seq_message_type_t type)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    ysw_hp_sort_sn_array(hp);

    uint32_t note_count = 0;
    ysw_note_t *notes = ysw_hp_get_notes(hp, &note_count);

    ysw_seq_message_t message = {
        .type = type,
        .stage.notes = notes,
        .stage.note_count = note_count,
        .stage.tempo = hp->tempo,
        .stage.on_status = (void *)on_sequencer_status,
        .stage.on_status_context = hpc,
    };

    seq_send(&message);
}

static void play(hpc_t *hpc)
{
    send_notes(hpc, YSW_SEQ_PLAY);
}

static void auto_play_all(hpc_t *hpc)
{
    switch (ysw_lv_hpe_gs.auto_play) {
        case YSW_AUTO_PLAY_OFF:
            break;
        case YSW_AUTO_PLAY_STAGE:
            send_notes(hpc, YSW_SEQ_STAGE);
            break;
        default:
            ESP_LOGE(TAG, "auto_play=%d not implemented", ysw_lv_hpe_gs.auto_play);
            break;
    }
}

static void stop()
{
    ysw_seq_message_t message = {
        .type = YSW_SEQ_STOP,
    };

    seq_send(&message);
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
    auto_play_all(hpc);
}

static void create_hp(hpc_t *hpc, uint32_t new_index)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);

    char name[HP_NAME_SZ];
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

static void copy_to_clipboard(hpc_t *hpc, ysw_ps_t *ps)
{
    ysw_ps_t *new_ps = ysw_ps_copy(ps);
    //ysw_ps_select(ps, false);
    ysw_ps_select(new_ps, true);
    ysw_array_push(hpc->clipboard, new_ps);
}

static void visit_pss(hpc_t *hpc, ps_visitor_t visitor)
{
    uint32_t visitor_count = 0;
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    uint32_t ps_count = ysw_hp_get_ps_count(hp);
    for (uint32_t i = 0; i < ps_count; i++) {
        ysw_ps_t *ps = ysw_hp_get_ps(hp, i);
        if (ysw_ps_is_selected(ps)) {
            visitor(hpc, ps);
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
    auto_play_all(hpc);
}

static void on_octave_change(hpc_t *hpc, uint8_t new_octave)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    hp->octave = new_octave;
    auto_play_all(hpc);
}

static void on_mode_change(hpc_t *hpc, ysw_mode_t new_mode)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    hp->mode = new_mode;
    auto_play_all(hpc);
}

static void on_transposition_change(hpc_t *hpc, uint8_t new_transposition_index)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    hp->transposition = ysw_transposition_from_index(new_transposition_index);
    auto_play_all(hpc);
}

static void on_tempo_change(hpc_t *hpc, uint8_t new_tempo_index)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    hp->tempo = ysw_tempo_from_index(new_tempo_index);
    update_footer(hpc);
    auto_play_all(hpc);
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
        ysw_seq_message_t message = {
            .type = YSW_SEQ_LOOP,
            .loop.loop = false,
        };
        seq_send(&message);
    } else {
        ysw_seq_message_t message = {
            .type = YSW_SEQ_LOOP,
            .loop.loop = true,
        };
        seq_send(&message);
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
    ESP_LOGD(TAG, "on_close hpc->close_cb=%p", hpc->close_cb);
    ysw_seq_message_t message = {
        .type = YSW_SEQ_STOP,
    };
    seq_rendezvous(&message);
    if (hpc->close_cb) {
        hpc->close_cb(hpc->close_cb_context, hpc);
    }
    ESP_LOGD(TAG, "on_close deleting hpc->frame");
    ysw_frame_del(hpc->frame); // deletes contents
    ESP_LOGD(TAG, "on_close freeing hpc");
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
    ysw_mfw_write(hpc->music);
}

static void on_new(hpc_t *hpc, lv_obj_t *btn)
{
    create_hp(hpc, hpc->hp_index + 1);
}

static void on_copy(hpc_t *hpc, lv_obj_t *btn)
{
    if (hpc->clipboard) {
        uint32_t old_ps_count = ysw_array_get_count(hpc->clipboard);
        for (int i = 0; i < old_ps_count; i++) {
            ysw_ps_t *old_ps = ysw_array_get(hpc->clipboard, i);
            ysw_ps_free(old_ps);
        }
        ysw_array_truncate(hpc->clipboard, 0);
    } else {
        hpc->clipboard = ysw_array_create(10);
    }
    visit_pss(hpc, copy_to_clipboard);
}

static void deselect_all(hpc_t *hpc)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    uint32_t ps_count = ysw_hp_get_ps_count(hp);
    for (uint32_t i = 0; i < ps_count; i++) {
        ysw_ps_t *ps = ysw_hp_get_ps(hp, i);
        ysw_ps_select(ps, false);
    }
}

static void on_paste(hpc_t *hpc, lv_obj_t *btn)
{
    ESP_LOGD(TAG, "on_paste entered");
    if (hpc->clipboard) {
        uint32_t paste_count = ysw_array_get_count(hpc->clipboard);
        if (paste_count) {
            deselect_all(hpc);
            uint32_t insert_index;
            ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
            uint32_t ps_count = ysw_hp_get_ps_count(hp);
            if (hpc->ps_index < ps_count) {
                // paste after most recently selected ps
                insert_index = hpc->ps_index + 1;
            } else {
                // paste at end (which is also beginning, if empty)
                insert_index = ps_count;
            }
            uint32_t first = insert_index;
            ESP_LOGD(TAG, "ps_index=%d, paste_count=%d, ps_count=%d, insert_index=%d", hpc->ps_index, paste_count, ps_count, insert_index);
            for (uint32_t i = 0; i < paste_count; i++) {
                ysw_ps_t *ps = ysw_array_get(hpc->clipboard, i);
                ysw_ps_t *new_ps = ysw_ps_copy(ps);
                new_ps->state = ps->state;
                ysw_hp_insert_ps(hp, insert_index, new_ps);
                hpc->ps_index = ++insert_index;
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
    uint32_t ps_count = ysw_hp_get_ps_count(hp);
    for (uint32_t source = 0; source < ps_count; source++) {
        ysw_ps_t *ps = ysw_hp_get_ps(hp, source);
        if (ysw_ps_is_selected(ps)) {
            ysw_ps_free(ps);
            changes++;
        } else {
            ysw_array_set(hp->pss, target, ps);
            target++;
        }
    }
    if (changes) {
        ysw_array_truncate(hp->pss, target);
        refresh(hpc);
    }
}

static void on_left(hpc_t *hpc, lv_obj_t *btn)
{
    uint32_t changes = 0;
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    uint32_t ps_count = ysw_hp_get_ps_count(hp);
    for (int32_t i = 0, j = 1; j < ps_count; i++, j++) {
        ysw_ps_t *hpc_ps = ysw_hp_get_ps(hp, j);
        if (ysw_ps_is_selected(hpc_ps)) {
            ysw_ps_t *other_ps = ysw_array_get(hp->pss, i);
            ysw_array_set(hp->pss, i, hpc_ps);
            ysw_array_set(hp->pss, j, other_ps);
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
    uint32_t ps_count = ysw_hp_get_ps_count(hp);
    for (int32_t i = ps_count - 1, j = ps_count - 2; j >= 0; i--, j--) {
        ysw_ps_t *hpc_ps = ysw_hp_get_ps(hp, j);
        if (ysw_ps_is_selected(hpc_ps)) {
            ysw_ps_t *other_ps = ysw_array_get(hp->pss, i);
            ysw_array_set(hp->pss, i, hpc_ps);
            ysw_array_set(hp->pss, j, other_ps);
            changes++;
        }
    }
    if (changes) {
        refresh(hpc);
    }
}

static void on_create_ps(hpc_t *hpc, uint32_t ps_index, uint8_t degree)
{
    if (ysw_music_get_cs_count(hpc->music)) {
        ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
        uint32_t ps_count = ysw_hp_get_ps_count(hp);
        ysw_cs_t *template_cs;
        if (hpc->ps_index < ps_count) {
            ysw_ps_t *last_ps = ysw_hp_get_ps(hp, hpc->ps_index);
            template_cs = last_ps->cs;
        } else {
            template_cs = ysw_music_get_cs(hpc->music, 0);
        }
        ysw_ps_t *ps = ysw_ps_create(template_cs, degree, YSW_PS_NEW_MEASURE);
        ps_count++;
        hpc->ps_index = ps_index;
        if (hpc->ps_index > ps_count) {
            hpc->ps_index = ps_count;
        }
        ysw_hp_insert_ps(hp, hpc->ps_index, ps);
        ysw_ps_select(ps, true);
        refresh(hpc);
    }
}

static void on_edit_ps(hpc_t *hpc, ysw_ps_t *ps)
{
    ysw_ps_select(ps, true);
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    hpc->ps_index = ysw_hp_get_ps_index(hp, ps);
    ysw_ssc_create(hpc->music, hp, hpc->ps_index);
}

static void on_select(hpc_t *hpc, ysw_ps_t *ps)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->music, hpc->hp_index);
    hpc->ps_index = ysw_hp_get_ps_index(hp, ps);
    ESP_LOGD(TAG, "on_select ps_index=%d", hpc->ps_index);
}

static void on_drag_end(hpc_t *hpc)
{
    auto_play_all(hpc);
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
    hpc->ps_index = 0;
    hpc->frame = create_frame(hpc);
    hpc->hpe = ysw_lv_hpe_create(hpc->frame->win, hpc);
    ysw_lv_hpe_set_create_cb(hpc->hpe, on_create_ps);
    ysw_lv_hpe_set_edit_cb(hpc->hpe, on_edit_ps);
    ysw_lv_hpe_set_select_cb(hpc->hpe, on_select);
    ysw_lv_hpe_set_drag_end_cb(hpc->hpe, on_drag_end);
    ysw_frame_set_content(hpc->frame, hpc->hpe);
    update_frame(hpc);
    return hpc;
}

void hpc_set_close_cb(hpc_t *hpc, hpc_close_cb_t cb, void *context)
{
    hpc->close_cb = cb;
    hpc->close_cb_context = context;
}

