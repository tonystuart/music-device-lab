// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_hpc.h"

#include "ysw_auto_play.h"
#include "ysw_hp.h"
#include "ysw_cs.h"
#include "ysw_sn.h"
#include "ysw_heap.h"
#include "ysw_instruments.h"
#include "ysw_hpe.h"
#include "ysw_mfw.h"
#include "ysw_mode.h"
#include "ysw_music.h"
#include "ysw_name.h"
#include "ysw_octaves.h"
#include "ysw_sdb.h"
#include "ysw_seq.h"
#include "ysw_main_seq.h"
#include "ysw_ssc.h"
#include "ysw_style.h"
#include "ysw_synth.h"
#include "ysw_tempo.h"
#include "ysw_transposition.h"

#include "lvgl/lvgl.h"

#include "esp_log.h"

#define TAG "YSW_HPC"

typedef void (*ps_visitor_t)(ysw_hpc_t *hpc, ysw_ps_t *ps);

// NB: invoked by sequencer task
static void on_sequencer_status(ysw_hpc_t *hpc, ysw_seq_status_message_t *message)
{
    if (message->type == YSW_SEQ_IDLE) {
        ysw_hpe_on_metro(hpc->controller.hpe, NULL);
    } else if (message->type == YSW_SEQ_NOTE) {
        ysw_hpe_on_metro(hpc->controller.hpe, message->note);
    }
}

static void send_steps(ysw_hpc_t *hpc, ysw_seq_message_type_t type)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    ysw_hp_sort_sn_array(hp);

    uint32_t note_count = 0;
    ysw_note_t *notes = ysw_hp_get_notes(hp, &note_count);

    ysw_seq_message_t message = {
        .type = type,
        .stage.notes = notes,
        .stage.note_count = note_count,
        .stage.tempo = hp->tempo,
        .stage.on_status = (void*)on_sequencer_status,
        .stage.on_status_context = hpc,
    };

    ysw_main_seq_send(&message);
}

static void send_step(ysw_hpc_t *hpc, ysw_ps_t *ps, ysw_seq_message_type_t type)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    ysw_hp_sort_sn_array(hp);

    uint32_t note_count = 0;
    ysw_note_t *notes = ysw_hp_get_step_notes(hp, ps, &note_count);

    ysw_seq_message_t message = {
        .type = type,
        .stage.notes = notes,
        .stage.note_count = note_count,
        .stage.tempo = hp->tempo,
        .stage.on_status = (void*)on_sequencer_status,
        .stage.on_status_context = hpc,
    };

    ysw_main_seq_send(&message);
}

static void play(ysw_hpc_t *hpc)
{
    send_steps(hpc, YSW_SEQ_PLAY);
}

static void auto_play_all(ysw_hpc_t *hpc)
{
    switch (ysw_hpe_gs.auto_play_all) {
        case YSW_AUTO_PLAY_OFF:
            break;
        case YSW_AUTO_PLAY_STAGE:
            send_steps(hpc, YSW_SEQ_STAGE);
            break;
        case YSW_AUTO_PLAY_PLAY:
            send_steps(hpc, YSW_SEQ_PLAY);
            break;
    }
}

static void auto_play_last(ysw_hpc_t *hpc, ysw_ps_t *ps)
{
    switch (ysw_hpe_gs.auto_play_last) {
        case YSW_AUTO_PLAY_OFF:
            break;
        case YSW_AUTO_PLAY_STAGE:
            send_step(hpc, ps, YSW_SEQ_STAGE);
            break;
        case YSW_AUTO_PLAY_PLAY:
            send_step(hpc, ps, YSW_SEQ_PLAY);
            break;
    }
}

static void update_header(ysw_hpc_t *hpc)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    ysw_ui_set_header_text(&hpc->frame.header, hp->name);
}

static void update_footer(ysw_hpc_t *hpc)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d BPM", hp->tempo);
    ysw_ui_set_footer_text(&hpc->frame.footer, buf);
}

static void update_frame(ysw_hpc_t *hpc)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    ysw_hpe_set_hp(hpc->controller.hpe, hp);
    update_header(hpc);
    update_footer(hpc);
}

static void refresh(ysw_hpc_t *hpc)
{
    lv_obj_invalidate(hpc->controller.hpe);
    auto_play_all(hpc);
}

static void create_hp(ysw_hpc_t *hpc, uint32_t new_index)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);

    char name[HP_NAME_SZ];
    ysw_name_create(name, sizeof(name));
    ysw_hp_t *new_hp = ysw_hp_create(name,
            hp->instrument,
            hp->octave,
            hp->mode,
            hp->transposition,
            hp->tempo);

    ysw_music_insert_hp(hpc->controller.music, new_index, new_hp);
    hpc->controller.hp_index = new_index;
    update_frame(hpc);
}

static void copy_to_clipboard(ysw_hpc_t *hpc, ysw_ps_t *ps)
{
    ysw_ps_t *new_ps = ysw_ps_copy(ps);
    //ysw_ps_select(ps, false);
    ysw_ps_select(new_ps, true);
    ysw_array_push(hpc->controller.clipboard, new_ps);
}

static void visit_pss(ysw_hpc_t *hpc, ps_visitor_t visitor)
{
    uint32_t visitor_count = 0;
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    uint32_t ps_count = ysw_hp_get_ps_count(hp);
    for (uint32_t i = 0; i < ps_count; i++) {
        ysw_ps_t *ps = ysw_hp_get_ps(hp, i);
        if (ysw_ps_is_selected(ps)) {
            visitor(hpc, ps);
            visitor_count++;
        }
    }
    if (visitor_count) {
        lv_obj_invalidate(hpc->controller.hpe);
    }
}

static void on_name_change(ysw_hpc_t *hpc, const char *new_name)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    ysw_hp_set_name(hp, new_name);
    update_header(hpc);
}

static void on_instrument_change(ysw_hpc_t *hpc, uint16_t new_instrument)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    ysw_hp_set_instrument(hp, new_instrument);
    auto_play_all(hpc);
}

static void on_octave_change(ysw_hpc_t *hpc, uint16_t new_octave)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    hp->octave = new_octave;
    auto_play_all(hpc);
}

static void on_mode_change(ysw_hpc_t *hpc, uint16_t new_mode)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    hp->mode = new_mode;
    auto_play_all(hpc);
}

static void on_transposition_change(ysw_hpc_t *hpc, uint16_t new_transposition_index)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    hp->transposition = ysw_transposition_from_index(new_transposition_index);
    auto_play_all(hpc);
}

static void on_tempo_change(ysw_hpc_t *hpc, uint16_t new_tempo_index)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    hp->tempo = ysw_tempo_from_index(new_tempo_index);
    update_footer(hpc);
    auto_play_all(hpc);
}

static void on_next(ysw_hpc_t *hpc, lv_obj_t *btn)
{
    uint32_t hp_count = ysw_music_get_hp_count(hpc->controller.music);
    if (++(hpc->controller.hp_index) >= hp_count) {
        hpc->controller.hp_index = 0;
    }
    update_frame(hpc);
}

static void on_play(ysw_hpc_t *hpc, lv_obj_t *btn)
{
    play(hpc);
}

static void on_prev(ysw_hpc_t *hpc, lv_obj_t *btn)
{
    uint32_t hp_count = ysw_music_get_hp_count(hpc->controller.music);
    if (--(hpc->controller.hp_index) >= hp_count) {
        hpc->controller.hp_index = hp_count - 1;
    }
    update_frame(hpc);
}

static void on_close(ysw_hpc_t *hpc, lv_obj_t *btn)
{
    ESP_LOGD(TAG, "on_close hpc->controller.close_cb=%p", hpc->controller.close_cb);
    ysw_seq_message_t message = {
        .type = YSW_SEQ_STOP,
    };
    ysw_main_seq_rendezvous(&message);
    if (hpc->controller.close_cb) {
        hpc->controller.close_cb(hpc->controller.close_cb_context, hpc);
    }
    ESP_LOGD(TAG, "on_close deleting hpc->frame");
    ysw_ui_close_frame(&hpc->frame); // deletes contents
    ESP_LOGD(TAG, "on_close freeing hpc");
    ysw_heap_free(hpc);
}

static void on_settings(ysw_hpc_t *hpc, lv_obj_t *btn)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    uint8_t trans_index = ysw_transposition_to_index(hp->transposition);
    uint8_t tempo_index = ysw_tempo_to_index(hp->tempo);
    ysw_sdb_t *sdb = ysw_sdb_create_standard(lv_scr_act(), "Chord Progression Settings", hpc);
    ysw_sdb_add_string(sdb, "Name:", hp->name, on_name_change);
    ysw_sdb_add_choice(sdb, "Instrument:", hp->instrument, ysw_instruments, on_instrument_change);
    ysw_sdb_add_choice(sdb, "Octave:", hp->octave, ysw_octaves, on_octave_change);
    ysw_sdb_add_choice(sdb, "Mode:", hp->mode, ysw_modes, on_mode_change);
    ysw_sdb_add_choice(sdb, "Transposition:", trans_index, ysw_transposition, on_transposition_change);
    ysw_sdb_add_choice(sdb, "Tempo:", tempo_index, ysw_tempo, on_tempo_change);
}

static void on_save(ysw_hpc_t *hpc, lv_obj_t *btn)
{
    ysw_mfw_write(hpc->controller.music);
}

static void on_new(ysw_hpc_t *hpc, lv_obj_t *btn)
{
    create_hp(hpc, hpc->controller.hp_index + 1);
}

static void on_copy(ysw_hpc_t *hpc, lv_obj_t *btn)
{
    if (hpc->controller.clipboard) {
        uint32_t old_ps_count = ysw_array_get_count(hpc->controller.clipboard);
        for (int i = 0; i < old_ps_count; i++) {
            ysw_ps_t *old_ps = ysw_array_get(hpc->controller.clipboard, i);
            ysw_ps_free(old_ps);
        }
        ysw_array_truncate(hpc->controller.clipboard, 0);
    } else {
        hpc->controller.clipboard = ysw_array_create(10);
    }
    visit_pss(hpc, copy_to_clipboard);
}

static void deselect_all(ysw_hpc_t *hpc)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    uint32_t ps_count = ysw_hp_get_ps_count(hp);
    for (uint32_t i = 0; i < ps_count; i++) {
        ysw_ps_t *ps = ysw_hp_get_ps(hp, i);
        ysw_ps_select(ps, false);
    }
}

static void on_paste(ysw_hpc_t *hpc, lv_obj_t *btn)
{
    ESP_LOGD(TAG, "on_paste entered");
    if (hpc->controller.clipboard) {
        uint32_t paste_count = ysw_array_get_count(hpc->controller.clipboard);
        if (paste_count) {
            deselect_all(hpc);
            uint32_t insert_index;
            ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
            uint32_t ps_count = ysw_hp_get_ps_count(hp);
            if (hpc->controller.ps_index < ps_count) {
                // paste after most recently selected ps
                insert_index = hpc->controller.ps_index + 1;
            } else {
                // paste at end (which is also beginning, if empty)
                insert_index = ps_count;
            }
            uint32_t first = insert_index;
            ESP_LOGD(TAG, "ps_index=%d, paste_count=%d, ps_count=%d, insert_index=%d", hpc->controller.ps_index, paste_count, ps_count, insert_index);
            for (uint32_t i = 0; i < paste_count; i++) {
                ysw_ps_t *ps = ysw_array_get(hpc->controller.clipboard, i);
                ysw_ps_t *new_ps = ysw_ps_copy(ps);
                new_ps->state = ps->state;
                ysw_hp_insert_ps(hp, insert_index, new_ps);
                hpc->controller.ps_index = ++insert_index;
            }
            ysw_hpe_ensure_visible(hpc->controller.hpe, first, first + paste_count - 1); // -1 because width is included
            ESP_LOGD(TAG, "on_paste calling refresh");
            refresh(hpc);
        }
    }
}

static void on_trash(ysw_hpc_t *hpc, lv_obj_t *btn)
{
    uint32_t target = 0;
    uint32_t changes = 0;
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    uint32_t ps_count = ysw_hp_get_ps_count(hp);
    for (uint32_t source = 0; source < ps_count; source++) {
        ysw_ps_t *ps = ysw_hp_get_ps(hp, source);
        if (ysw_ps_is_selected(ps)) {
            ysw_ps_free(ps);
            changes++;
        } else {
            ysw_array_set(hp->ps_array, target, ps);
            target++;
        }
    }
    if (changes) {
        ysw_array_truncate(hp->ps_array, target);
        refresh(hpc);
    }
}

static void on_left(ysw_hpc_t *hpc, lv_obj_t *btn)
{
    uint32_t changes = 0;
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    uint32_t ps_count = ysw_hp_get_ps_count(hp);
    for (int32_t i = 0, j = 1; j < ps_count; i++, j++) {
        ysw_ps_t *hpc_ps = ysw_hp_get_ps(hp, j);
        if (ysw_ps_is_selected(hpc_ps)) {
            ysw_ps_t *other_ps = ysw_array_get(hp->ps_array, i);
            ysw_array_set(hp->ps_array, i, hpc_ps);
            ysw_array_set(hp->ps_array, j, other_ps);
            changes++;
        }
    }
    if (changes) {
        refresh(hpc);
    }
}

static void on_right(ysw_hpc_t *hpc, lv_obj_t *btn)
{
    uint32_t changes = 0;
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    uint32_t ps_count = ysw_hp_get_ps_count(hp);
    for (int32_t i = ps_count - 1, j = ps_count - 2; j >= 0; i--, j--) {
        ysw_ps_t *hpc_ps = ysw_hp_get_ps(hp, j);
        if (ysw_ps_is_selected(hpc_ps)) {
            ysw_ps_t *other_ps = ysw_array_get(hp->ps_array, i);
            ysw_array_set(hp->ps_array, i, hpc_ps);
            ysw_array_set(hp->ps_array, j, other_ps);
            changes++;
        }
    }
    if (changes) {
        refresh(hpc);
    }
}

static void on_create_ps(ysw_hpc_t *hpc, uint32_t ps_index, uint8_t degree)
{
    if (ysw_music_get_cs_count(hpc->controller.music)) {
        ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
        uint32_t ps_count = ysw_hp_get_ps_count(hp);
        ysw_cs_t *template_cs;
        if (hpc->controller.ps_index < ps_count) {
            ysw_ps_t *last_ps = ysw_hp_get_ps(hp, hpc->controller.ps_index);
            template_cs = last_ps->cs;
        } else {
            template_cs = ysw_music_get_cs(hpc->controller.music, 0); // TODO: what if null?
        }
        ysw_ps_t *ps = ysw_ps_create(template_cs, degree, YSW_PS_NEW_MEASURE);
        ps_count++;
        hpc->controller.ps_index = ps_index;
        if (hpc->controller.ps_index > ps_count) {
            hpc->controller.ps_index = ps_count;
        }
        ysw_hp_insert_ps(hp, hpc->controller.ps_index, ps);
        ysw_ps_select(ps, true);
        auto_play_last(hpc, ps);
        refresh(hpc);
        uint32_t first = ps_index > 0 ? ps_index - 1 : 0;
        uint32_t last = ps_index < ps_count - 1 ? ps_index + 1 : ps_index;
        ysw_hpe_ensure_visible(hpc->controller.hpe, first, last);
    }
}

static void on_edit_ps(ysw_hpc_t *hpc, ysw_ps_t *ps)
{
    ESP_LOGE(TAG, "on_edit_ps entered");
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    hpc->controller.ps_index = ysw_hp_get_ps_index(hp, ps);
    ysw_ssc_create(hpc->controller.music, hp, hpc->controller.ps_index, false);
}

static void on_select(ysw_hpc_t *hpc, ysw_ps_t *ps)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpc->controller.music, hpc->controller.hp_index);
    hpc->controller.ps_index = ysw_hp_get_ps_index(hp, ps);
    ESP_LOGD(TAG, "on_select ps_index=%d", hpc->controller.ps_index);
    auto_play_last(hpc, ps);
}

static void on_drag_end(ysw_hpc_t *hpc)
{
    auto_play_all(hpc);
}

static const ysw_ui_btn_def_t header_buttons[] = {
    { LV_SYMBOL_PREV, on_prev },
    { LV_SYMBOL_PLAY, on_play },
    { LV_SYMBOL_STOP, ysw_main_seq_on_stop },
    { LV_SYMBOL_LOOP, ysw_main_seq_on_loop },
    { LV_SYMBOL_NEXT, on_next },
    { LV_SYMBOL_CLOSE, on_close },
    { NULL, NULL },
};

static const ysw_ui_btn_def_t footer_buttons[] = {
    { LV_SYMBOL_SETTINGS, on_settings },
    { LV_SYMBOL_SAVE, on_save },
    { LV_SYMBOL_AUDIO, on_new }, // TODO: add NEW button
    { LV_SYMBOL_COPY, on_copy },
    { LV_SYMBOL_PASTE, on_paste },
    { LV_SYMBOL_TRASH, on_trash },
    { LV_SYMBOL_LEFT, on_left },
    { LV_SYMBOL_RIGHT, on_right },
    { NULL, NULL },
};

static void create_hpe(ysw_hpc_t *hpc)
{
    hpc->controller.hpe = ysw_hpe_create(hpc->frame.body.page, hpc);
    lv_coord_t w = lv_page_get_width_fit(hpc->frame.body.page);
    lv_coord_t h = lv_page_get_height_fit(hpc->frame.body.page);
    lv_obj_set_size(hpc->controller.hpe, w, h);
    ysw_style_adjust_obj(hpc->controller.hpe);
    ysw_hpe_set_create_cb(hpc->controller.hpe, on_create_ps);
    ysw_hpe_set_edit_cb(hpc->controller.hpe, on_edit_ps);
    ysw_hpe_set_select_cb(hpc->controller.hpe, on_select);
    ysw_hpe_set_drag_end_cb(hpc->controller.hpe, on_drag_end);
}

ysw_hpc_t* ysw_hpc_create(lv_obj_t *parent, ysw_music_t *music, uint32_t hp_index)
{
    ysw_hpc_t *hpc = ysw_heap_allocate(sizeof(ysw_hpc_t)); // freed in on_close

    hpc->controller.music = music;
    hpc->controller.hp_index = hp_index;
    hpc->controller.ps_index = 0;

    ysw_ui_init_buttons(hpc->frame.header.buttons, header_buttons, hpc);
    ysw_ui_init_buttons(hpc->frame.footer.buttons, footer_buttons, hpc);

    ysw_ui_create_frame(&hpc->frame, parent);

    create_hpe(hpc);

    update_frame(hpc);

    return hpc;
}

void ysw_hpc_set_close_cb(ysw_hpc_t *hpc, hpc_close_cb_t cb, void *context)
{
    hpc->controller.close_cb = cb;
    hpc->controller.close_cb_context = context;
}

