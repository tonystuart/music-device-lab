// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_csc.h"

#include "ysw_auto_play.h"
#include "ysw_cs.h"
#include "ysw_sn.h"
#include "ysw_division.h"
#include "ysw_heap.h"
#include "ysw_instruments.h"
#include "ysw_cse.h"
#include "ysw_mfw.h"
#include "ysw_mode.h"
#include "ysw_music.h"
#include "ysw_name.h"
#include "ysw_octaves.h"
#include "ysw_sdb.h"
#include "ysw_seq.h"
#include "ysw_main_seq.h"
#include "ysw_style.h"
#include "ysw_synth.h"
#include "ysw_tempo.h"
#include "ysw_transposition.h"
#include "ysw_ui.h"

#include "lvgl/lvgl.h"

#include "esp_log.h"

#define TAG "YSW_CSC"

typedef void (*sn_visitor_t)(ysw_csc_t *csc, ysw_sn_t *sn);

// NB: invoked by sequencer task
static void on_sequencer_status(ysw_csc_t *csc, ysw_seq_status_message_t *message)
{
    if (message->type == YSW_SEQ_IDLE) {
        ysw_cse_on_metro(csc->controller.cse, NULL);
    } else if (message->type == YSW_SEQ_NOTE) {
        ysw_cse_on_metro(csc->controller.cse, message->note);
    }
}

static void send_notes(ysw_csc_t *csc, ysw_seq_message_type_t type)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    ysw_cs_sort_sn_array(cs);

    uint32_t note_count = 0;
    ysw_note_t *notes = ysw_cs_get_notes(cs, &note_count);

    ysw_seq_message_t message = {
        .type = type,
        .stage.notes = notes,
        .stage.note_count = note_count,
        .stage.tempo = cs->tempo,
        .stage.on_status = (void*)on_sequencer_status,
        .stage.on_status_context = csc,
    };

    ysw_main_seq_send(&message);
}

static void send_note(ysw_csc_t *csc, ysw_sn_t *sn, ysw_seq_message_type_t type)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    ysw_note_t *note = ysw_cs_get_note(cs, sn);

    ysw_seq_message_t message = {
        .type = type,
        .stage.notes = note,
        .stage.note_count = 1,
        .stage.tempo = 80,
    };

    ysw_main_seq_send(&message);
}

static void play(ysw_csc_t *csc)
{
    send_notes(csc, YSW_SEQ_PLAY);
}

static void auto_play_all(ysw_csc_t *csc)
{
    switch (ysw_cse_gs.auto_play_all) {
        case YSW_AUTO_PLAY_OFF:
            break;
        case YSW_AUTO_PLAY_STAGE:
            send_notes(csc, YSW_SEQ_STAGE);
            break;
        case YSW_AUTO_PLAY_PLAY:
            send_notes(csc, YSW_SEQ_PLAY);
            break;
    }
}

static void auto_play_last(ysw_csc_t *csc, ysw_sn_t *sn)
{
    switch (ysw_cse_gs.auto_play_last) {
        case YSW_AUTO_PLAY_OFF:
            break;
        case YSW_AUTO_PLAY_STAGE:
            send_note(csc, sn, YSW_SEQ_STAGE);
            break;
        case YSW_AUTO_PLAY_PLAY:
            send_note(csc, sn, YSW_SEQ_PLAY);
            break;
    }
}

static void update_header(ysw_csc_t *csc)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    ysw_ui_set_header_text(&csc->frame.header, cs->name);
}

static void update_footer(ysw_csc_t *csc)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d BPM %s", cs->tempo, ysw_division_to_tick(cs->divisions));
    ysw_ui_set_footer_text(&csc->frame.footer, buf);
}

static void update_frame(ysw_csc_t *csc)
{
    uint32_t cs_count = ysw_music_get_cs_count(csc->controller.music);
    if (csc->controller.cs_index < cs_count) {
        ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
        ysw_cse_set_cs(csc->controller.cse, cs);
        update_header(csc);
        update_footer(csc);
    }
}

static void refresh(ysw_csc_t *csc)
{
    lv_obj_invalidate(csc->controller.cse);
    auto_play_all(csc);
}

static ysw_cs_t* create_cs(ysw_csc_t *csc)
{
    char name[64];
    ysw_name_create(name, sizeof(name));

    ysw_cs_t *new_cs;
    uint32_t new_index;

    uint32_t cs_count = ysw_music_get_cs_count(csc->controller.music);

    ESP_LOGD(TAG, "cs_count=%d, csc->controller.cs_index=%d", cs_count, csc->controller.cs_index);

    if (csc->controller.cs_index < cs_count) {
        ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
        new_cs = ysw_cs_create(name,
                cs->instrument,
                cs->octave,
                cs->mode,
                cs->transposition,
                cs->tempo,
                cs->divisions);
        new_index = csc->controller.cs_index + 1;
    } else {
        new_cs = ysw_cs_create(name,
                0,
                4,
                0,
                0,
                120,
                4);
        new_index = 0;
    }

    ESP_LOGD(TAG, "new_index=%d", new_index);

    ysw_music_insert_cs(csc->controller.music, new_index, new_cs);
    csc->controller.cs_index = new_index;
    update_frame(csc);
    return new_cs;
}

static void copy_to_clipboard(ysw_csc_t *csc, ysw_sn_t *sn)
{
    ysw_sn_t *new_sn = ysw_sn_copy(sn);
    //ysw_sn_select(sn, false);
    ysw_sn_select(new_sn, true);
    ysw_array_push(csc->controller.clipboard, new_sn);
}

static void free_clipboard_contents(ysw_csc_t *csc)
{
    uint32_t old_sn_count = ysw_array_get_count(csc->controller.clipboard);
    for (int i = 0; i < old_sn_count; i++) {
        ysw_sn_t *old_sn = ysw_array_get(csc->controller.clipboard, i);
        ysw_sn_free(old_sn);
    }
}

static void free_clipboard(ysw_csc_t *csc)
{
    free_clipboard_contents(csc);
    ysw_heap_free(csc->controller.clipboard);
    csc->controller.clipboard = NULL;
}

static void truncate_clipboard(ysw_csc_t *csc)
{
    free_clipboard_contents(csc);
    ysw_array_truncate(csc->controller.clipboard, 0);
}

static void decrease_volume(ysw_csc_t *csc, ysw_sn_t *sn)
{
    int new_velocity = ((sn->velocity - 1) / 10) * 10;
    if (new_velocity >= 0) {
        sn->velocity = new_velocity;
    }
}

static void increase_volume(ysw_csc_t *csc, ysw_sn_t *sn)
{
    int new_velocity = ((sn->velocity + 10) / 10) * 10;
    if (new_velocity <= 120) {
        sn->velocity = new_velocity;
    }
}

static void visit_sn(ysw_csc_t *csc, sn_visitor_t visitor)
{
    uint32_t visitor_count = 0;
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    uint32_t sn_count = ysw_cs_get_sn_count(cs);
    for (uint32_t i = 0; i < sn_count; i++) {
        ysw_sn_t *sn = ysw_cs_get_sn(cs, i);
        if (ysw_sn_is_selected(sn)) {
            visitor(csc, sn);
            visitor_count++;
        }
    }
    if (visitor_count) {
        lv_obj_invalidate(csc->controller.cse);
    }
}

static void on_name_change(ysw_csc_t *csc, const char *new_name)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    ysw_cs_set_name(cs, new_name);
    update_header(csc);
}

static void on_instrument_change(ysw_csc_t *csc, uint8_t new_instrument)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    ysw_cs_set_instrument(cs, new_instrument);
    auto_play_all(csc);
}

static void on_octave_change(ysw_csc_t *csc, uint8_t new_octave)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    cs->octave = new_octave;
    auto_play_all(csc);
}

static void on_mode_change(ysw_csc_t *csc, ysw_mode_t new_mode)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    cs->mode = new_mode;
    auto_play_all(csc);
}

static void on_transposition_change(ysw_csc_t *csc, uint8_t new_transposition_index)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    cs->transposition = ysw_transposition_from_index(new_transposition_index);
    auto_play_all(csc);
}

static void on_tempo_change(ysw_csc_t *csc, uint8_t new_tempo_index)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    cs->tempo = ysw_tempo_from_index(new_tempo_index);
    update_footer(csc);
    auto_play_all(csc);
}

static void on_division_change(ysw_csc_t *csc, uint8_t new_division_index)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    cs->divisions = ysw_division_from_index(new_division_index);
    update_footer(csc);
    auto_play_all(csc);
}

static void on_next(ysw_csc_t *csc, lv_obj_t *btn)
{
    uint32_t cs_count = ysw_music_get_cs_count(csc->controller.music);
    if (++(csc->controller.cs_index) >= cs_count) {
        csc->controller.cs_index = 0;
    }
    update_frame(csc);
}

static void on_play(ysw_csc_t *csc, lv_obj_t *btn)
{
    play(csc);
}

static void on_prev(ysw_csc_t *csc, lv_obj_t *btn)
{
    uint32_t cs_count = ysw_music_get_cs_count(csc->controller.music);
    if (--(csc->controller.cs_index) >= cs_count) {
        csc->controller.cs_index = cs_count - 1;
    }
    update_frame(csc);
}

static void on_close(ysw_csc_t *csc, lv_obj_t *btn)
{
    ESP_LOGD(TAG, "on_close csc->controller.close_cb=%p", csc->controller.close_cb);
    ysw_seq_message_t message = {
        .type = YSW_SEQ_STOP,
    };
    ysw_main_seq_rendezvous(&message);
    if (csc->controller.clipboard) {
        // TODO: be sure that ysw_hpc also frees clipboard
        free_clipboard(csc);
    }
    ysw_ui_close_frame(&csc->frame);
    if (csc->controller.close_cb) {
        csc->controller.close_cb(csc->controller.close_cb_context, csc->controller.cs_index);
    }
    ysw_heap_free(csc);
}

static void on_settings(ysw_csc_t *csc, lv_obj_t *btn)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    uint8_t trans_index = ysw_transposition_to_index(cs->transposition);
    uint8_t tempo_index = ysw_tempo_to_index(cs->tempo);
    uint8_t division_index = ysw_division_to_index(cs->divisions);
    ysw_sdb_t *sdb = ysw_sdb_create_standard(lv_scr_act(), "Chord Style Settings", csc);
    ysw_sdb_add_string(sdb, "Name:", cs->name, on_name_change);
    ysw_sdb_add_choice(sdb, "Instrument:", cs->instrument, ysw_instruments, on_instrument_change);
    ysw_sdb_add_choice(sdb, "Octave:", cs->octave, ysw_octaves, on_octave_change);
    ysw_sdb_add_choice(sdb, "Mode:", cs->mode, ysw_modes, on_mode_change);
    ysw_sdb_add_choice(sdb, "Transposition:", trans_index, ysw_transposition, on_transposition_change);
    ysw_sdb_add_choice(sdb, "Tempo:", tempo_index, ysw_tempo, on_tempo_change);
    ysw_sdb_add_choice(sdb, "Divisions:", division_index, ysw_division, on_division_change);
}

static void on_save(ysw_csc_t *csc, lv_obj_t *btn)
{
    ysw_mfw_write(csc->controller.music);
}

static void on_new(ysw_csc_t *csc, lv_obj_t *btn)
{
    create_cs(csc);
}

static void on_copy(ysw_csc_t *csc, lv_obj_t *btn)
{
    if (csc->controller.clipboard) {
        truncate_clipboard(csc);
    } else {
        csc->controller.clipboard = ysw_array_create(10);
    }
    visit_sn(csc, copy_to_clipboard);
}

static void deselect_all(ysw_csc_t *csc)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    uint32_t sn_count = ysw_cs_get_sn_count(cs);
    for (uint32_t i = 0; i < sn_count; i++) {
        ysw_sn_t *sn = ysw_cs_get_sn(cs, i);
        ysw_sn_select(sn, false);
    }
}

static void on_paste(ysw_csc_t *csc, lv_obj_t *btn)
{
    if (csc->controller.clipboard) {
        deselect_all(csc);
        ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
        uint32_t sn_count = ysw_array_get_count(csc->controller.clipboard);
        for (uint32_t i = 0; i < sn_count; i++) {
            ysw_sn_t *sn = ysw_array_get(csc->controller.clipboard, i);
            ysw_sn_t *new_sn = ysw_sn_copy(sn);
            new_sn->state = sn->state;
            ysw_cs_add_sn(cs, new_sn);
        }
        refresh(csc);
    }
}

static void on_trash(ysw_csc_t *csc, lv_obj_t *btn)
{
    uint32_t target = 0;
    uint32_t changes = 0;
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    uint32_t sn_count = ysw_cs_get_sn_count(cs);
    for (uint32_t source = 0; source < sn_count; source++) {
        ysw_sn_t *sn = ysw_array_get(cs->sn_array, source);
        if (ysw_sn_is_selected(sn)) {
            ysw_sn_free(sn);
            changes++;
        } else {
            ysw_array_set(cs->sn_array, target, sn);
            target++;
        }
    }
    if (changes) {
        ysw_array_truncate(cs->sn_array, target);
        refresh(csc);
    }
}

static void on_volume_mid(ysw_csc_t *csc, lv_obj_t *btn)
{
    visit_sn(csc, decrease_volume);
}

static void on_volume_max(ysw_csc_t *csc, lv_obj_t *btn)
{
    visit_sn(csc, increase_volume);
}

static void on_create_sn(ysw_csc_t *csc, uint32_t start, int8_t degree)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->controller.music, csc->controller.cs_index);
    uint32_t division_duration = YSW_CS_DURATION / cs->divisions;
    uint32_t start_floor = (start / division_duration) * division_duration;
    uint32_t duration = division_duration - 5;
    ysw_sn_t *sn = ysw_sn_create(degree, 80, start_floor, duration, 0);
    ysw_sn_select(sn, true);
    ysw_cs_add_sn(cs, sn);
    auto_play_last(csc, sn);
    refresh(csc);
}

static void on_edit_sn(ysw_csc_t *csc, ysw_sn_t *sn)
{
    ESP_LOGD(TAG, "on_edit_sn stub");
}

static void on_select(ysw_csc_t *csc, ysw_sn_t *sn)
{
    auto_play_last(csc, sn);
}

static void on_drag_end(ysw_csc_t *csc)
{
    auto_play_all(csc);
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
    { LV_SYMBOL_VOLUME_MID, on_volume_mid },
    { LV_SYMBOL_VOLUME_MAX, on_volume_max },
    { NULL, NULL },
};

static void create_cse(ysw_csc_t *csc)
{
    csc->controller.cse = ysw_cse_create(csc->frame.body.page, csc);
    lv_coord_t w = lv_page_get_width_fit(csc->frame.body.page);
    lv_coord_t h = lv_page_get_height_fit(csc->frame.body.page);
    lv_obj_set_size(csc->controller.cse, w, h);
    ysw_style_adjust_obj(csc->controller.cse);
    ysw_cse_set_create_cb(csc->controller.cse, on_create_sn);
    ysw_cse_set_edit_cb(csc->controller.cse, on_edit_sn);
    ysw_cse_set_select_cb(csc->controller.cse, on_select);
    ysw_cse_set_drag_end_cb(csc->controller.cse, on_drag_end);
}

ysw_csc_t* ysw_csc_create(lv_obj_t *parent, ysw_music_t *music, uint32_t cs_index)
{
    ysw_csc_t *csc = ysw_heap_allocate(sizeof(ysw_csc_t)); // freed in on_close

    csc->controller.music = music;
    csc->controller.cs_index = cs_index;

    ysw_ui_init_buttons(csc->frame.header.buttons, header_buttons, csc);
    ysw_ui_init_buttons(csc->frame.footer.buttons, footer_buttons, csc);

    ysw_ui_create_frame(&csc->frame, parent);

    create_cse(csc);

    update_frame(csc);

    return csc;
}

ysw_csc_t* ysw_csc_create_new(lv_obj_t *parent, ysw_music_t *music, uint32_t cs_index)
{
    ysw_csc_t *csc = ysw_csc_create(parent, music, cs_index);
    create_cs(csc);
    return csc;
}

void ysw_csc_set_close_cb(ysw_csc_t *csc, void *cb, void *context)
{
    csc->controller.close_cb = cb;
    csc->controller.close_cb_context = context;
}

