// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "csc.h"

#include "sequencer.h"

#include "ysw_cs.h"
#include "ysw_cn.h"
#include "ysw_division.h"
#include "ysw_heap.h"
#include "ysw_instruments.h"
#include "ysw_lv_cse.h"
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

#define TAG "CSC"

typedef void (*cn_visitor_t)(csc_t *csc, ysw_cn_t *cn);

static void send_notes(csc_t *csc, ysw_sequencer_message_type_t type)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
    ysw_cs_sort_cn_array(cs);

    uint32_t note_count = 0;
    note_t *notes = ysw_cs_get_notes(cs, &note_count);

    ysw_sequencer_message_t message = {
        .type = type,
        .stage.notes = notes,
        .stage.note_count = note_count,
        .stage.tempo = cs->tempo,
    };

    sequencer_send(&message);
}

static void play(csc_t *csc)
{
    send_notes(csc, YSW_SEQUENCER_PLAY);
}

static void stage(csc_t *csc)
{
    if (ysw_lv_cse_gs.auto_play) {
        send_notes(csc, YSW_SEQUENCER_STAGE);
    }
}

static void stop()
{
    ysw_sequencer_message_t message = {
        .type = YSW_SEQUENCER_PAUSE,
    };

    sequencer_send(&message);
}

static void update_header(csc_t *csc)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
    ysw_frame_set_header_text(csc->frame, cs->name);
}

static void update_footer(csc_t *csc)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d BPM %s", cs->tempo, ysw_division_to_tick(cs->divisions));
    ysw_frame_set_footer_text(csc->frame, buf);
}

static void update_frame(csc_t *csc)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
    ysw_lv_cse_set_cs(csc->cse, cs);
    update_header(csc);
    update_footer(csc);
}

static void refresh(csc_t *csc)
{
    lv_obj_invalidate(csc->cse);
    stage(csc);
}

static ysw_cs_t *create_cs(csc_t *csc, uint32_t new_index)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);

    char name[64];
    ysw_name_create(name, sizeof(name));
    ysw_cs_t *new_cs = ysw_cs_create(name,
            cs->instrument,
            cs->octave,
            cs->mode,
            cs->transposition,
            cs->tempo,
            cs->divisions);

    ysw_music_insert_cs(csc->music, new_index, new_cs);
    csc->cs_index = new_index;
    update_frame(csc);
    return new_cs;
}

static void copy_to_clipboard(csc_t *csc, ysw_cn_t *cn)
{
    ysw_cn_t *new_cn = ysw_cn_copy(cn);
    ysw_cn_select(cn, false);
    ysw_cn_select(new_cn, true);
    ysw_array_push(csc->clipboard, new_cn);
}

static void decrease_volume(csc_t *csc, ysw_cn_t *cn)
{
    int new_velocity = ((cn->velocity - 1) / 10) * 10;
    if (new_velocity >= 0) {
        cn->velocity = new_velocity;
    }
}

static void increase_volume(csc_t *csc, ysw_cn_t *cn)
{
    int new_velocity = ((cn->velocity + 10) / 10) * 10;
    if (new_velocity <= 120) {
        cn->velocity = new_velocity;
    }
}

static void visit_cn(csc_t *csc, cn_visitor_t visitor)
{
    uint32_t visitor_count = 0;
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
    uint32_t cn_count = ysw_cs_get_cn_count(cs);
    for (uint32_t i = 0; i < cn_count; i++) {
        ysw_cn_t *cn = ysw_cs_get_cn(cs, i);
        if (ysw_cn_is_selected(cn)) {
            visitor(csc, cn);
            visitor_count++;
        }
    }
    if (visitor_count) {
        lv_obj_invalidate(csc->cse);
    }
}

static void on_name_change(csc_t *csc, const char *new_name)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
    ysw_cs_set_name(cs, new_name);
    update_header(csc);
}

static void on_instrument_change(csc_t *csc, uint8_t new_instrument)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
    ysw_cs_set_instrument(cs, new_instrument);
    stage(csc);
}

static void on_octave_change(csc_t *csc, uint8_t new_octave)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
    cs->octave = new_octave;
    stage(csc);
}

static void on_mode_change(csc_t *csc, ysw_mode_t new_mode)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
    cs->mode = new_mode;
    stage(csc);
}

static void on_transposition_change(csc_t *csc, uint8_t new_transposition_index)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
    cs->transposition = ysw_transposition_from_index(new_transposition_index);
    stage(csc);
}

static void on_tempo_change(csc_t *csc, uint8_t new_tempo_index)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
    cs->tempo = ysw_tempo_from_index(new_tempo_index);
    update_footer(csc);
    stage(csc);
}

static void on_division_change(csc_t *csc, uint8_t new_division_index)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
    cs->divisions = ysw_division_from_index(new_division_index);
    update_footer(csc);
    stage(csc);
}

static void on_next(csc_t *csc, lv_obj_t * btn, lv_event_t event)
{
    uint32_t cs_count = ysw_music_get_cs_count(csc->music);
    if (++(csc->cs_index) >= cs_count) {
        csc->cs_index = 0;
    }
    update_frame(csc);
}

static void on_play(csc_t *csc, lv_obj_t * btn, lv_event_t event)
{
    play(csc);
}

static void on_stop(csc_t *csc, lv_obj_t * btn, lv_event_t event)
{
    stop();
}

static void on_loop(csc_t *csc, lv_obj_t * btn, lv_event_t event)
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

static void on_prev(csc_t *csc, lv_obj_t * btn, lv_event_t event)
{
    uint32_t cs_count = ysw_music_get_cs_count(csc->music);
    if (--(csc->cs_index) >= cs_count) {
        csc->cs_index = cs_count - 1;
    }
    update_frame(csc);
}

static void on_close(csc_t *csc, lv_obj_t * btn, lv_event_t event)
{
    if (csc->close_cb) {
        csc->close_cb(csc->close_cb_context, csc);
    }
    ysw_frame_del(csc->frame); // deletes contents
    ysw_heap_free(csc);
}

static void on_settings(csc_t *csc, lv_obj_t * btn, lv_event_t event)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
    uint8_t trans_index = ysw_transposition_to_index(cs->transposition);
    uint8_t tempo_index = ysw_tempo_to_index(cs->tempo);
    uint8_t division_index = ysw_division_to_index(cs->divisions);
    ysw_sdb_t *sdb = ysw_sdb_create("Chord Style Settings", csc);
    ysw_sdb_add_string(sdb, "Name", cs->name, on_name_change);
    ysw_sdb_add_choice(sdb, "Instrument", cs->instrument, ysw_instruments, on_instrument_change);
    ysw_sdb_add_choice(sdb, "Octave", cs->octave, ysw_octaves, on_octave_change);
    ysw_sdb_add_choice(sdb, "Mode", cs->mode, ysw_modes, on_mode_change);
    ysw_sdb_add_choice(sdb, "Transposition", trans_index, ysw_transposition, on_transposition_change);
    ysw_sdb_add_choice(sdb, "Tempo", tempo_index, ysw_tempo, on_tempo_change);
    ysw_sdb_add_choice(sdb, "Divisions", division_index, ysw_division, on_division_change);
}

static void on_save(csc_t *csc, lv_obj_t * btn, lv_event_t event)
{
}

static void on_new(csc_t *csc, lv_obj_t * btn, lv_event_t event)
{
    create_cs(csc, csc->cs_index + 1);
}

static void on_copy(csc_t *csc, lv_obj_t * btn, lv_event_t event)
{
    if (csc->clipboard) {
        uint32_t old_cn_count = ysw_array_get_count(csc->clipboard);
        for (int i = 0; i < old_cn_count; i++) {
            ysw_cn_t *old_cn = ysw_array_get(csc->clipboard, i);
            ysw_cn_free(old_cn);
        }
        ysw_array_truncate(csc->clipboard, 0);
    } else {
        csc->clipboard = ysw_array_create(10);
    }
    visit_cn(csc, copy_to_clipboard);
}

static void on_paste(csc_t *csc, lv_obj_t * btn, lv_event_t event)
{
    if (csc->clipboard) {
        ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
        uint32_t cn_count = ysw_array_get_count(csc->clipboard);
        for (uint32_t i = 0; i < cn_count; i++) {
            ysw_cn_t *cn = ysw_array_get(csc->clipboard, i);
            ysw_cn_t *new_cn = ysw_cn_copy(cn);
            new_cn->state = cn->state;
            ysw_cs_add_cn(cs, new_cn);
        }
        if (cn_count) {
            refresh(csc);
        }
    }
}

static void on_trash(csc_t *csc, lv_obj_t * btn, lv_event_t event)
{
    uint32_t target = 0;
    uint32_t changes = 0;
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
    uint32_t cn_count = ysw_cs_get_cn_count(cs);
    for (uint32_t source = 0; source < cn_count; source++) {
        ysw_cn_t *cn = ysw_array_get(cs->cn_array, source);
        if (ysw_cn_is_selected(cn)) {
            ysw_cn_free(cn);
            changes++;
        } else {
            ysw_array_set(cs->cn_array, target, cn);
            target++;
        }
    }
    if (changes) {
        ysw_array_truncate(cs->cn_array, target);
        refresh(csc);
    }
}

static void on_volume_mid(csc_t *csc, lv_obj_t * btn)
{
    visit_cn(csc, decrease_volume);
}

static void on_volume_max(csc_t *csc, lv_obj_t * btn, lv_event_t event)
{
    visit_cn(csc, increase_volume);
}

static void on_create_cn(csc_t *csc, int8_t degree, uint32_t start)
{
    ysw_cs_t *cs = ysw_music_get_cs(csc->music, csc->cs_index);
    uint32_t duration = (YSW_CS_DURATION / cs->divisions) - 5;
    ysw_cn_t *cn = ysw_cn_create(degree, 80, start, duration, 0);
    ysw_cs_add_cn(cs, cn);
    refresh(csc);
}

static ysw_frame_t *create_frame(csc_t *csc)
{
    ysw_frame_t *frame = ysw_frame_create(csc);

    ysw_frame_add_footer_button(frame, LV_SYMBOL_SETTINGS, on_settings);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_SAVE, on_save);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_AUDIO, on_new);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_COPY, on_copy);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_PASTE, on_paste);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_TRASH, on_trash);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_VOLUME_MID, on_volume_mid);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_VOLUME_MAX, on_volume_max);

    ysw_frame_add_header_button(frame, LV_SYMBOL_CLOSE, on_close);
    ysw_frame_add_header_button(frame, LV_SYMBOL_NEXT, on_next);
    ysw_frame_add_header_button(frame, LV_SYMBOL_LOOP, on_loop);
    ysw_frame_add_header_button(frame, LV_SYMBOL_STOP, on_stop);
    ysw_frame_add_header_button(frame, LV_SYMBOL_PLAY, on_play);
    ysw_frame_add_header_button(frame, LV_SYMBOL_PREV, on_prev);

    return frame;
}

csc_t *csc_create(ysw_music_t *music, uint32_t cs_index)
{
    csc_t *csc = ysw_heap_allocate(sizeof(csc_t)); // freed in on_close
    csc->music = music;
    csc->cs_index = cs_index;
    csc->frame = create_frame(csc);
    csc->cse = ysw_lv_cse_create(csc->frame->win, csc);
    ysw_lv_cse_set_create_cb(csc->cse, on_create_cn);
    ysw_lv_cse_set_drag_end_cb(csc->cse, stage);
    ysw_frame_set_content(csc->frame, csc->cse);
    update_frame(csc);
    return csc;
}

csc_t *csc_create_new(ysw_music_t *music, uint32_t old_cs_index)
{
    csc_t *csc = csc_create(music, old_cs_index);
    create_cs(csc, old_cs_index + 1);
    update_frame(csc);
    return csc;
}

void csc_set_close_cb(csc_t *csc, void *cb, void *context)
{
    csc->close_cb = cb;
    csc->close_cb_context = context;
}

void csc_on_metro(csc_t *csc, note_t *metro_note)
{
    ysw_lv_cse_on_metro(csc->cse, metro_note);
}

