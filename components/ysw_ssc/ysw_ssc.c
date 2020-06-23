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
#include "ysw_hp.h"
#include "ysw_sdb.h"
#include "ysw_main_seq.h"
#include "lvgl/lvgl.h"
#include "esp_log.h"
#include "stdio.h"

#define TAG "YSW_SSC"

// NB: returned value is allocated on ysw_heap. Caller must free it.

static char *get_chord_styles(ssc_t *ssc, uint32_t *cs_index)
{
    uint32_t length = 0;
    *cs_index = 0;
    uint32_t cs_count = ysw_music_get_cs_count(ssc->music);
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_cs_t *cs = ysw_music_get_cs(ssc->music, i);
        length += strlen(cs->name) + 1; // +1 due to \n delimiter or null terminator
        if (cs == ssc->ps->cs) {
            *cs_index = i;
        }
    }
    ESP_LOGD(TAG, "chord_styles length=%d", length);
    char *chord_styles = ysw_heap_allocate(length);
    char *p = chord_styles;
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_cs_t *cs = ysw_music_get_cs(ssc->music, i);
        char *n = cs->name;
        if (i) {
            *p++ = '\n';
        }
        while (*n) {
            *p++ = *n++;
        }
    }
    *p++ = EOS;
    ESP_LOGD(TAG, "chord_styles=%s, used_length=%d (%d)", chord_styles, strlen(chord_styles) + RFT, p-chord_styles);
    return chord_styles;
}

static void on_new_measure(ssc_t *ssc, uint8_t new_measure)
{
    ysw_ps_set_new_measure(ssc->ps, new_measure);
}

static void on_edit_style(ssc_t *ssc)
{
    uint32_t cs_index = ysw_music_get_cs_index(ssc->music, ssc->ps->cs);
    ysw_csc_create(lv_scr_act(), ssc->music, cs_index);
}

static void on_csc_close(ssc_t *ssc, ysw_csc_t *csc)
{
    uint32_t cs_index = 0;
    char *chord_styles = get_chord_styles(ssc, &cs_index);
    ESP_LOGD(TAG, "on_csc_close chord_styles=%s", chord_styles);
    lv_dropdown_set_options(ssc->styles, chord_styles);
    lv_dropdown_set_selected(ssc->styles, cs_index);
    ysw_heap_free(chord_styles);
}

static void on_create_style(ssc_t *ssc)
{
    uint32_t cs_index = ysw_music_get_cs_index(ssc->music, ssc->ps->cs);
    ysw_csc_t *csc = ysw_csc_create_new(lv_scr_act(), ssc->music, cs_index);
    ysw_csc_set_close_cb(csc, on_csc_close, ssc);
    ssc->ps->cs = ysw_music_get_cs(ssc->music, cs_index + 1);
}

static void on_apply_all(ssc_t *ssc)
{
    uint32_t ps_count = ysw_hp_get_ps_count(ssc->hp);
    for (uint32_t i = 0; i < ps_count; i++) {
        ysw_ps_t *ps = ysw_hp_get_ps(ssc->hp, i);
        ps->cs = ssc->ps->cs;
    }
}

static void on_apply_selected(ssc_t *ssc)
{
    uint32_t ps_count = ysw_hp_get_ps_count(ssc->hp);
    for (uint32_t i = 0; i < ps_count; i++) {
        ysw_ps_t *ps = ysw_hp_get_ps(ssc->hp, i);
        if (ysw_ps_is_selected(ps)) {
            ps->cs = ssc->ps->cs;
        }
    }
}

static void on_degree(ssc_t *ssc, uint8_t new_index)
{
    ssc->ps->degree = ysw_degree_from_index(new_index);
}

static void on_chord_style(ssc_t *ssc, uint8_t new_index)
{
    ssc->ps->cs = ysw_music_get_cs(ssc->music, new_index);
}

void ysw_ssc_create(ysw_music_t *music, ysw_hp_t *hp, uint32_t ps_index)
{
    ssc_t *ssc = ysw_heap_allocate(sizeof(ssc_t)); // TODO: make sure this gets freed
    ssc->music = music;
    ssc->hp = hp;
    ssc->ps = ysw_hp_get_ps(hp, ps_index);

    uint32_t cs_index = 0;
    char *chord_styles = get_chord_styles(ssc, &cs_index);

    char title[64];
    uint32_t ps_count = ysw_hp_get_ps_count(hp);
    snprintf(title, sizeof(title), "Step %d of %d", ps_index + 1, ps_count);

    ysw_sdb_t *sdb = ysw_sdb_create(lv_scr_act(), title, ssc);

#if 0
    // TODO: Encapsulate properly within ysw_sdb
    lv_win_add_btn(sdb->win, LV_SYMBOL_NEXT);
    ysw_main_seq_init_loop_btn(lv_win_add_btn(sdb->win, LV_SYMBOL_LOOP));
    lv_win_add_btn(sdb->win, LV_SYMBOL_STOP);
    lv_win_add_btn(sdb->win, LV_SYMBOL_PLAY);
    lv_win_add_btn(sdb->win, LV_SYMBOL_PREV);
#endif

    ysw_sdb_add_separator(sdb, hp->name);
    ysw_sdb_add_choice(sdb, "Degree:", ysw_degree_to_index(ssc->ps->degree), ysw_degree, on_degree);
    ysw_sdb_add_choice(sdb, "New Measure:", ysw_ps_is_new_measure(ssc->ps), "No\nYes", on_new_measure);
    ssc->styles = ysw_sdb_add_choice(sdb, "Chord Style:", cs_index, chord_styles, on_chord_style);
    ysw_sdb_add_button(sdb, "Edit Chord Style", on_edit_style);
    ysw_sdb_add_button(sdb, "Create Chord Style", on_create_style);
    ysw_sdb_add_button(sdb, "Apply Chord Style to All Steps", on_apply_all);
    ysw_sdb_add_button(sdb, "Apply Style to Selected Steps", on_apply_selected);

    ysw_heap_free(chord_styles); // choice uses ddlist, which uses label, which allocs space
}

