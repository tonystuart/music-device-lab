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
#include "ysw_main_seq.h"
#include "lvgl/lvgl.h"
#include "esp_log.h"
#include "stdio.h"

#define TAG "YSW_SSC"

#define EDIT "Edit"
#define CREATE "Create"
#define APPLY_ALL "Apply All"
#define APPLY_SELECTED "Apply Selected"

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

static void on_new_measure(ysw_ssc_t *ssc, uint8_t new_measure)
{
    ysw_ps_set_new_measure(ssc->model.ps, new_measure);
}

static void on_csc_close(ysw_ssc_model_t *model, uint32_t cs_index)
{
    uint32_t cs_count = ysw_music_get_cs_count(model->music);
    if (cs_index < cs_count) {
        model->ps->cs = ysw_music_get_cs(model->music, cs_index);
    }
    int32_t ps_index = ysw_hp_get_ps_index(model->hp, model->ps);
    if (ps_index == -1) {
        ps_index = 0;
    }
    ysw_ssc_create(model->music, model->hp, ps_index);
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

static void on_apply_all(ysw_ssc_t *ssc)
{
    uint32_t ps_count = ysw_hp_get_ps_count(ssc->model.hp);
    for (uint32_t i = 0; i < ps_count; i++) {
        ysw_ps_t *ps = ysw_hp_get_ps(ssc->model.hp, i);
        ps->cs = ssc->model.ps->cs;
    }
}

static void on_apply_selected(ysw_ssc_t *ssc)
{
    uint32_t ps_count = ysw_hp_get_ps_count(ssc->model.hp);
    for (uint32_t i = 0; i < ps_count; i++) {
        ysw_ps_t *ps = ysw_hp_get_ps(ssc->model.hp, i);
        if (ysw_ps_is_selected(ps)) {
            ps->cs = ssc->model.ps->cs;
        }
    }
}

static void on_degree(ysw_ssc_t *ssc, uint8_t new_index)
{
    ssc->model.ps->degree = ysw_degree_from_index(new_index);
}

static void on_chord_style(ysw_ssc_t *ssc, uint8_t new_index)
{
    ssc->model.ps->cs = ysw_music_get_cs(ssc->model.music, new_index);
}

static void on_chord_style_action(ysw_ssc_t *ssc, const char *button)
{
    if (strcmp(button, EDIT) == 0) {
        on_edit_style(ssc);
    } else if (strcmp(button, CREATE) == 0) {
        on_create_style(ssc);
    } else if (strcmp(button, APPLY_ALL) == 0) {
        on_apply_all(ssc);
    } else if (strcmp(button, APPLY_SELECTED) == 0) {
        on_apply_selected(ssc);
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

static void on_prev(ysw_sdb_t *sdb, lv_obj_t *btn)
{
    // TODO: if multiple selection, move amongst selection, otherwise move selection
    ysw_ssc_t *ssc = sdb->controller.context; // TODO: figure out whether SDB should take care of this
    uint32_t ps_count = ysw_hp_get_ps_count(ssc->model.hp);
    if (ps_count) {
        uint32_t ps_index = ysw_hp_get_ps_index(ssc->model.hp, ssc->model.ps);
        int32_t new_ps_index = ps_index - 1;
        if (new_ps_index < 0) {
            new_ps_index = ps_count - 1;
        }
        update_settings(ssc, new_ps_index);
    }
}

static void on_play(ysw_ssc_t *ssc, lv_obj_t *btn)
{
}

static void on_next(ysw_sdb_t *sdb, lv_obj_t *btn)
{
    // TODO: if multiple selection, move amongst selection, otherwise move selection
    ysw_ssc_t *ssc = sdb->controller.context; // TODO: figure out whether SDB should take care of this
    uint32_t ps_count = ysw_hp_get_ps_count(ssc->model.hp);
    if (ps_count) {
        uint32_t ps_index = ysw_hp_get_ps_index(ssc->model.hp, ssc->model.ps);
        uint32_t new_ps_index = ps_index + 1;
        if (new_ps_index == ps_count) {
            new_ps_index = 0;
        }
        update_settings(ssc, new_ps_index);
    }
}

static void on_close(ysw_sdb_t *sdb, lv_obj_t *btn)
{
    ysw_ssc_t *ssc = sdb->controller.context; // TODO: figure out whether SDB should take care of this
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

static const char *map[] = { EDIT, CREATE, APPLY_ALL, "" };

void ysw_ssc_create(ysw_music_t *music, ysw_hp_t *hp, uint32_t ps_index)
{
    ysw_ssc_t *ssc = ysw_heap_allocate(sizeof(ysw_ssc_t)); // TODO: make sure this gets freed
    ssc->model.music = music;
    ssc->model.hp = hp;
    ssc->model.ps = ysw_hp_get_ps(hp, ps_index);

    uint32_t cs_index = 0;
    char *chord_styles = get_chord_styles(ssc, &cs_index);

    char text[64];
    get_step_number_text(hp, ps_index, text, sizeof(text));

    ssc->view.sdb = ysw_sdb_create_custom(lv_scr_act(), text, header_buttons, ssc);
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
