// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_nsc.h"
#include "ysw_common.h"
#include "ysw_cse.h"
#include "ysw_csc.h"
#include "ysw_heap.h"
#include "ysw_main_bus.h"
#include "ysw_main_seq.h"
#include "ysw_quatone.h"
#include "ysw_value.h"
#include "lvgl/lvgl.h"
#include "esp_log.h"
#include "assert.h"
#include "stdio.h"

#define TAG "YSW_NSC"

typedef void (*note_visitor_cb)(ysw_sn_t *sn, ysw_value_t value);

typedef enum {
    FORWARD,
    BACKWARD,
} move_direction_t;

static void get_note_number_text(ysw_cs_t *cs, uint32_t sn_index, char *text, uint32_t size)
{
    uint32_t sn_count = ysw_cs_get_sn_count(cs);
    snprintf(text, size, "Note %d of %d", sn_index + 1, sn_count);
}

static void update_settings(ysw_nsc_t *nsc, uint32_t sn_index)
{
    char text[64];
    get_note_number_text(nsc->model.cs, sn_index, text, sizeof(text));
    ysw_ui_set_header_text(&nsc->view.sdb->frame.header, text);
    nsc->model.sn = ysw_cs_get_sn(nsc->model.cs, sn_index);
    ysw_sdb_set_number(nsc->view.start, nsc->model.sn->start);
    ysw_sdb_set_number(nsc->view.duration, nsc->model.sn->duration);
    ysw_sdb_set_number(nsc->view.velocity, nsc->model.sn->velocity);
    lv_dropdown_set_selected(nsc->view.quatone, nsc->model.sn->quatone);
    lv_obj_invalidate(nsc->view.quatone);
}

static void move(ysw_nsc_t *nsc, move_direction_t direction)
{
    uint32_t sn_count = ysw_cs_get_sn_count(nsc->model.cs);
    if (sn_count >= 2) { // can only move if at least two items
        int32_t new_sn_index = -1;
        uint32_t sn_index = ysw_cs_get_sn_index(nsc->model.cs, nsc->model.sn);
        if (ysw_cse_gs.multiple_selection) {
            if (direction == FORWARD) {
                new_sn_index = ysw_cs_find_next_selected_sn(nsc->model.cs, sn_index, true);
            } else {
                new_sn_index = ysw_cs_find_previous_selected_sn(nsc->model.cs, sn_index, true);
            }
        } // not else
        if (new_sn_index == -1) { // single selection, or only one item selected
            if (direction == FORWARD) {
                new_sn_index = sn_index + 1;
                if (new_sn_index >= sn_count) {
                    new_sn_index = 0;
                }
            } else {
                new_sn_index = sn_index - 1;
                if (new_sn_index < 0) {
                    new_sn_index = sn_count - 1;
                }
            }
            ysw_sn_select(nsc->model.sn, false);
            ysw_sn_select(ysw_cs_get_sn(nsc->model.cs, new_sn_index), true);
            ysw_main_bus_publish(YSW_BUS_EVT_SEL_STEP, (void*)new_sn_index);
        }
        update_settings(nsc, new_sn_index);
    }
}

static void visit_notes(ysw_nsc_model_t *model, note_visitor_cb cb, ysw_value_t value)
{
    if (model->apply_all || ysw_cse_gs.multiple_selection) {
        uint32_t sn_count = ysw_cs_get_sn_count(model->cs);
        for (uint32_t i = 0; i < sn_count; i++) {
            ysw_sn_t *sn = ysw_cs_get_sn(model->cs, i);
            if (model->apply_all || ysw_sn_is_selected(sn)) {
                cb(sn, value);
            }
        }
    } else {
        cb(model->sn, value);
    }
}

static void new_start_visitor(ysw_sn_t *sn, ysw_value_t value)
{
    sn->start = min(value.ui, YSW_CS_DURATION - sn->duration);
}

static void new_duration_visitor(ysw_sn_t *sn, ysw_value_t value)
{
    sn->duration = min(value.ui, YSW_CS_DURATION - sn->start);
}

static void new_velocity_visitor(ysw_sn_t *sn, ysw_value_t value)
{
    sn->velocity = range(value.ui, 0, YSW_MIDI_MAX);
}

static void quatone_visitor(ysw_sn_t *sn, ysw_value_t value)
{
    sn->quatone = value.us;
}

static void on_new_start(ysw_nsc_t *nsc, int32_t new_start)
{
    visit_notes(&nsc->model, new_start_visitor, (ysw_value_t ) { .ui = new_start });
}

static void on_new_duration(ysw_nsc_t *nsc, int32_t new_duration)
{
    visit_notes(&nsc->model, new_duration_visitor, (ysw_value_t ) { .ui = new_duration });
}

static void on_new_velocity(ysw_nsc_t *nsc, int32_t new_velocity)
{
    visit_notes(&nsc->model, new_velocity_visitor, (ysw_value_t ) { .ui = new_velocity });
}

static void on_new_quatone(ysw_nsc_t *nsc, uint16_t new_index)
{
    ysw_value_t value = {
        .us = new_index,
    };
    visit_notes(&nsc->model, quatone_visitor, value);
}

static void on_apply_all(ysw_nsc_t *nsc, bool apply_all)
{
    nsc->model.apply_all = apply_all;
}

static void on_prev(ysw_nsc_t *nsc, lv_obj_t *btn)
{
    move(nsc, BACKWARD);
}

static void on_play(ysw_nsc_t *nsc, lv_obj_t *btn)
{
    ysw_cs_sort_sn_array(nsc->model.cs);

    ysw_note_t *notes = ysw_cs_get_note(nsc->model.cs, nsc->model.sn);

    ysw_seq_message_t message = {
        .type = YSW_SEQ_PLAY,
        .stage.notes = notes,
        .stage.note_count = 1,
        .stage.tempo = nsc->model.cs->tempo,
    };

    ysw_main_seq_send(&message);
}

static void on_next(ysw_nsc_t *nsc, lv_obj_t *btn)
{
    move(nsc, FORWARD);
}

static void on_close(ysw_nsc_t *nsc, lv_obj_t *btn)
{
    ysw_nsc_close(nsc);
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

void on_start_number_ta_cb(ysw_nsc_t *nsc, int32_t *min_value, int32_t *max_value, int32_t *step)
{
    *min_value = 0;
    *max_value = YSW_CS_DURATION - nsc->model.sn->duration;
    *step = 1;
}

void on_duration_number_ta_cb(ysw_nsc_t *nsc, int32_t *min_value, int32_t *max_value, int32_t *step)
{
    *min_value = YSW_CSN_MIN_DURATION;
    *max_value = YSW_CS_DURATION - nsc->model.sn->start;
    *step = 1;
}

void on_velocity_number_ta_cb(ysw_nsc_t *nsc, int32_t *min_value, int32_t *max_value, int32_t *step)
{
    *min_value = 0;
    *max_value = YSW_MIDI_MAX;
    *step = 1;
}

void ysw_nsc_create(ysw_music_t *music, ysw_cs_t *cs, uint32_t sn_index, bool apply_all)
{
    ysw_sn_t *sn = ysw_cs_get_sn(cs, sn_index);;

    ysw_nsc_t *nsc = ysw_heap_allocate(sizeof(ysw_nsc_t)); // freed in ysw_nsc_close
    nsc->model.music = music;
    nsc->model.cs = cs;
    nsc->model.sn = sn;
    nsc->model.apply_all = apply_all;

    char text[64];
    get_note_number_text(cs, sn_index, text, sizeof(text));

    nsc->view.sdb = ysw_sdb_create_custom(text, header_buttons, nsc);
    ysw_sdb_add_checkbox(nsc->view.sdb, NULL, " Apply changes to all Notes", nsc->model.apply_all, on_apply_all);
    nsc->view.start = ysw_sdb_add_number(nsc->view.sdb, "Note Start Time:", sn->start, on_start_number_ta_cb, on_new_start);
    nsc->view.duration = ysw_sdb_add_number(nsc->view.sdb, "Note Duration:", sn->duration, on_duration_number_ta_cb, on_new_duration);
    nsc->view.velocity = ysw_sdb_add_number(nsc->view.sdb, "Velocity (Loudness or Volume):", sn->velocity, on_velocity_number_ta_cb, on_new_velocity);
    nsc->view.quatone = ysw_sdb_add_choice(nsc->view.sdb, "Note:", sn->quatone, ysw_quatone_choices, on_new_quatone);
}

void ysw_nsc_close(ysw_nsc_t *nsc)
{
    ysw_sdb_on_close(nsc->view.sdb, NULL);
    ysw_heap_free(nsc);
}
