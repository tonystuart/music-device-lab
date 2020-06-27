// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_ssc.h"
#include "ysw_common.h"
#include "ysw_csc.h"
#include "ysw_degree.h"
#include "ysw_heap.h"
#include "ysw_hpe.h"
#include "ysw_main_seq.h"
#include "lvgl/lvgl.h"
#include "esp_log.h"
#include "assert.h"
#include "stdio.h"

#define TAG "YSW_SSC"

#define EDIT "Edit/Select"
#define CREATE "Create/Select"

typedef union {
    bool b;
    int16_t ss;
    uint16_t us;
    int32_t si;
    uint32_t ui;
    void *p;
} value_t;

typedef void (*step_visitor_cb)(ysw_ps_t *ps, value_t value);

// NB: returned value is allocated on ysw_heap. Caller must free it.

static char* get_chord_styles(ysw_ssc_t *ssc, uint32_t *cs_index)
{
    uint32_t length = 0;
    *cs_index = 0;
    uint32_t cs_count = ysw_music_get_cs_count(ssc->model.music);
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_cs_t *cs = ysw_music_get_cs(ssc->model.music, i);
        length += strlen(cs->name) + 1; // +1 due to \n delimiter or null terminator
        if (cs == ssc->model.ps->cs) {
            *cs_index = i;
        }
    }
    ESP_LOGD(TAG, "chord_styles length=%d", length);
    char *chord_styles = ysw_heap_allocate(length);
    char *p = chord_styles;
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_cs_t *cs = ysw_music_get_cs(ssc->model.music, i);
        char *n = cs->name;
        if (i) {
            *p++ = '\n';
        }
        while (*n) {
            *p++ = *n++;
        }
    }
    *p++ = EOS;
    ESP_LOGD(TAG, "chord_styles=%s, used_length=%d (%d)", chord_styles, strlen(chord_styles) + RFT, p - chord_styles);
    return chord_styles;
}

static void visit_steps(ysw_ssc_model_t *model, step_visitor_cb cb, value_t value)
{
    if (model->apply_all || ysw_hpe_gs.multiple_selection) {
        uint32_t ps_count = ysw_hp_get_ps_count(model->hp);
        for (uint32_t i = 0; i < ps_count; i++) {
            ysw_ps_t *ps = ysw_hp_get_ps(model->hp, i);
            if (model->apply_all || ysw_ps_is_selected(ps)) {
                cb(ps, value);
            }
        }
    } else {
        cb(model->ps, value);
    }
}

static void new_measure_visitor(ysw_ps_t *ps, value_t value)
{
    ysw_ps_set_new_measure(ps, value.b);
}

static void on_new_measure(ysw_ssc_t *ssc, bool new_measure)
{
    visit_steps(&ssc->model, new_measure_visitor, (value_t ) { .b = new_measure });
}

static void degree_visitor(ysw_ps_t *ps, value_t value)
{
    ps->degree = value.us;
}

static void on_degree(ysw_ssc_t *ssc, uint16_t new_index)
{
    value_t value = {
        .us = ysw_degree_from_index(new_index),
    };
    visit_steps(&ssc->model, degree_visitor, value);
}

static void chord_style_visitor(ysw_ps_t *ps, value_t value)
{
    ps->cs = value.p;
}

static void on_chord_style(ysw_ssc_t *ssc, uint16_t new_index)
{
    value_t value = {
        .p = ysw_music_get_cs(ssc->model.music, new_index),
    };
    visit_steps(&ssc->model, chord_style_visitor, value);
}

static void on_apply_all(ysw_ssc_t *ssc, bool apply_all)
{
    ssc->model.apply_all = apply_all;
}

static void on_csc_close(ysw_ssc_model_t *model, uint32_t cs_index)
{
    uint32_t cs_count = ysw_music_get_cs_count(model->music);
    if (cs_index < cs_count) {
        value_t value = {
            .p = ysw_music_get_cs(model->music, cs_index),
        };
        visit_steps(model, chord_style_visitor, value);
    }
    int32_t ps_index = ysw_hp_get_ps_index(model->hp, model->ps);
    if (ps_index == -1) {
        ps_index = 0;
    }
    ysw_ssc_create(model->music, model->hp, ps_index, model->apply_all);
    ysw_heap_free(model);
}

static void on_edit_style(ysw_ssc_t *ssc)
{
    ysw_ssc_model_t *model = ysw_heap_allocate(sizeof(ysw_ssc_model_t));
    *model = ssc->model;
    ysw_ssc_close(ssc); // no ref to ssc after this point
    uint32_t cs_index = ysw_music_get_cs_index(model->music, model->ps->cs);
    ysw_csc_t *csc = ysw_csc_create(lv_scr_act(), model->music, cs_index);
    ysw_csc_set_close_cb(csc, on_csc_close, model);
}

static void on_create_style(ysw_ssc_t *ssc)
{
    ysw_ssc_model_t *model = ysw_heap_allocate(sizeof(ysw_ssc_model_t));
    *model = ssc->model;
    ysw_ssc_close(ssc); // no ref to ssc after this point
    uint32_t cs_index = ysw_music_get_cs_index(model->music, model->ps->cs);
    ysw_csc_t *csc = ysw_csc_create_new(lv_scr_act(), model->music, cs_index);
    ysw_csc_set_close_cb(csc, on_csc_close, model);
}

static void on_chord_style_action(ysw_ssc_t *ssc, const char *button)
{
    if (strcmp(button, EDIT) == 0) {
        on_edit_style(ssc);
    } else if (strcmp(button, CREATE) == 0) {
        on_create_style(ssc);
    }
}

static void get_step_number_text(ysw_hp_t *hp, uint32_t ps_index, char *text, uint32_t size)
{
    uint32_t ps_count = ysw_hp_get_ps_count(hp);
    snprintf(text, size, "Step %d of %d", ps_index + 1, ps_count);
}

static void update_settings(ysw_ssc_t *ssc, uint32_t ps_index)
{
    char text[64];
    get_step_number_text(ssc->model.hp, ps_index, text, sizeof(text));
    ysw_ui_set_header_text(&ssc->view.sdb->frame.header, text);
    ssc->model.ps = ysw_hp_get_ps(ssc->model.hp, ps_index);
    lv_checkbox_set_checked(ssc->view.som, ysw_ps_is_new_measure(ssc->model.ps));
    lv_dropdown_set_selected(ssc->view.degree, ysw_degree_to_index(ssc->model.ps->degree));
    lv_dropdown_set_selected(ssc->view.styles, ysw_music_get_cs_index(ssc->model.music, ssc->model.ps->cs));
    lv_obj_invalidate(ssc->view.degree);
    lv_obj_invalidate(ssc->view.styles);
}

typedef enum {
    FORWARD,
    BACKWARD,
} move_direction_t;

static void move(ysw_ssc_t *ssc, move_direction_t direction)
{
    uint32_t ps_count = ysw_hp_get_ps_count(ssc->model.hp);
    if (ps_count >= 2) { // can only move if at least two items
        int32_t new_ps_index = -1;
        uint32_t ps_index = ysw_hp_get_ps_index(ssc->model.hp, ssc->model.ps);
        if (ysw_hpe_gs.multiple_selection) {
            if (direction == FORWARD) {
                new_ps_index = ysw_hp_find_next_selected_ps(ssc->model.hp, ps_index, true);
            } else {
                new_ps_index = ysw_hp_find_previous_selected_ps(ssc->model.hp, ps_index, true);
            }
        } // not else
        if (new_ps_index == -1) { // single selection, or only one item selected
            if (direction == FORWARD) {
                new_ps_index = ps_index + 1;
                if (new_ps_index >= ps_count) {
                    new_ps_index = 0;
                }
            } else {
                new_ps_index = ps_index - 1;
                if (new_ps_index < 0) {
                    new_ps_index = ps_count - 1;
                }
            }
            ysw_ps_select(ssc->model.ps, false);
            ysw_ps_select(ysw_hp_get_ps(ssc->model.hp, new_ps_index), true);
        }
        update_settings(ssc, new_ps_index);
    }
}

static void on_prev(ysw_ssc_t *ssc, lv_obj_t *btn)
{
    move(ssc, BACKWARD);
}

static void on_play(ysw_ssc_t *ssc, lv_obj_t *btn)
{
    ysw_hp_sort_sn_array(ssc->model.hp);

    uint32_t note_count = 0;
    ysw_note_t *notes = ysw_hp_get_step_notes(ssc->model.hp, ssc->model.ps, &note_count);

    ysw_seq_message_t message = {
        .type = YSW_SEQ_PLAY,
        .stage.notes = notes,
        .stage.note_count = note_count,
        .stage.tempo = ssc->model.hp->tempo,
    };

    ysw_main_seq_send(&message);
}

static void on_next(ysw_ssc_t *ssc, lv_obj_t *btn)
{
    move(ssc, FORWARD);
}

static void on_close(ysw_ssc_t *ssc, lv_obj_t *btn)
{
    ysw_ssc_close(ssc);
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

static const char *map[] = { EDIT, CREATE, "" };

void ysw_ssc_create(ysw_music_t *music, ysw_hp_t *hp, uint32_t ps_index, bool apply_all)
{
    ysw_ssc_t *ssc = ysw_heap_allocate(sizeof(ysw_ssc_t)); // freed in ysw_ssc_close
    ssc->model.music = music;
    ssc->model.hp = hp;
    ssc->model.ps = ysw_hp_get_ps(hp, ps_index);
    ssc->model.apply_all = apply_all;

    uint32_t cs_index = 0;
    char *chord_styles = get_chord_styles(ssc, &cs_index);

    char text[64];
    get_step_number_text(hp, ps_index, text, sizeof(text));

    ssc->view.sdb = ysw_sdb_create_custom(lv_scr_act(), text, header_buttons, ssc);
    ssc->view.som = ysw_sdb_add_checkbox(ssc->view.sdb, NULL, " Apply changes to all Steps", ssc->model.apply_all, on_apply_all);
    ssc->view.som = ysw_sdb_add_checkbox(ssc->view.sdb, NULL, " Start new Measure on this Step", ysw_ps_is_new_measure(ssc->model.ps), on_new_measure);
    ssc->view.degree = ysw_sdb_add_choice(ssc->view.sdb, "Step Degree:", ysw_degree_to_index(ssc->model.ps->degree), ysw_degree, on_degree);
    ssc->view.styles = ysw_sdb_add_choice(ssc->view.sdb, "Chord Style:", cs_index, chord_styles, on_chord_style);
    ysw_sdb_add_button_bar(ssc->view.sdb, "Chord Style Actions:", map, on_chord_style_action);

    ysw_heap_free(chord_styles);
}

void ysw_ssc_close(ysw_ssc_t *ssc)
{
    ysw_sdb_on_close(ssc->view.sdb, NULL);
    ysw_heap_free(ssc);
}
