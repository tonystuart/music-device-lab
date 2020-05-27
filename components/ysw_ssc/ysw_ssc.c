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
#include "ysw_degree.h"
#include "ysw_cp.h"
#include "ysw_sdb.h"
#include "ysw_heap.h"
#include "../../main/csc.h" // TODO: Fix this when we extract callbacks
#include "lvgl/lvgl.h"
#include "esp_log.h"
#include "stdio.h"

#define TAG "YSW_SSC"

// TODO: Move these to caller and configure callbacks via config_t
ysw_music_t *g_music;
ysw_cp_t *g_cp;
ysw_step_t *g_step;

static void on_new_measure(uint8_t new_value)
{
    ESP_LOGD(TAG, "on_new_measure before=%#x", g_step->flags);
    ysw_step_set_new_measure(g_step, new_value);
    ESP_LOGD(TAG, "on_new_measure after=%#x", g_step->flags);
}

static void on_edit_style(void)
{
    uint32_t cs_index = ysw_music_get_cs_index(g_music, g_step->cs);
    csc_edit(g_music, cs_index);
}

static void on_create_style(void)
{
    uint32_t cs_index = ysw_music_get_cs_index(g_music, g_step->cs);
    g_step->cs = csc_create(g_music, cs_index + 1);
    // TODO: Set ddlist index to cs_index + 1
}

static void on_apply_all(void)
{
    uint32_t step_count = ysw_cp_get_step_count(g_cp);
    for (uint32_t i = 0; i < step_count; i++) {
        ysw_step_t *step = ysw_cp_get_step(g_cp, i);
        step->cs = g_step->cs;
    }
}

static void on_apply_selected(void)
{
    uint32_t step_count = ysw_cp_get_step_count(g_cp);
    for (uint32_t i = 0; i < step_count; i++) {
        ysw_step_t *step = ysw_cp_get_step(g_cp, i);
        if (ysw_step_is_selected(step)) {
            step->cs = g_step->cs;
        }
    }
}

static void on_degree(uint8_t new_index)
{
    g_step->degree = ysw_degree_from_index(new_index);
}

static void on_chord_style(uint8_t new_index)
{
    g_step->cs = ysw_music_get_cs(g_music, new_index);
}

void ysw_ssc_create(ysw_music_t *music, ysw_cp_t *cp, uint32_t step_index)
{
    g_music = music;
    g_cp = cp;
    g_step = ysw_cp_get_step(cp, step_index);
    uint32_t length = 0;
    uint32_t cs_index = 0;
    uint32_t cs_count = ysw_music_get_cs_count(music);
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_cs_t *cs = ysw_music_get_cs(music, i);
        length += strlen(cs->name) + 1; // +1 due to \n delimiter or null terminator
        if (cs == g_step->cs) {
            cs_index = i;
        }
    }
    ESP_LOGD(TAG, "chord_styles length=%d", length);
    char *chord_styles = ysw_heap_allocate(length);
    char *p = chord_styles;
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_cs_t *cs = ysw_music_get_cs(music, i);
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
    char title[64];
    uint32_t step_count = ysw_cp_get_step_count(cp);
    snprintf(title, sizeof(title), "Step %d of %d", step_index + 1, step_count);
    ysw_sdb_t *sdb = ysw_sdb_create(title);
    // TODO: Encapsulate properly within ysw_sdb
    lv_win_add_btn(sdb->win, LV_SYMBOL_NEXT);
    lv_win_add_btn(sdb->win, LV_SYMBOL_LOOP);
    lv_win_add_btn(sdb->win, LV_SYMBOL_STOP);
    lv_win_add_btn(sdb->win, LV_SYMBOL_PLAY);
    lv_win_add_btn(sdb->win, LV_SYMBOL_PREV);
    ysw_sdb_add_separator(sdb, cp->name);
    ysw_sdb_add_choice(sdb, "Degree", ysw_degree_to_index(g_step->degree), ysw_degree, on_degree);
    ysw_sdb_add_choice(sdb, "New Measure", ysw_step_is_new_measure(g_step), "No\nYes", on_new_measure);
    ysw_sdb_add_choice(sdb, "Chord Style", cs_index, chord_styles, on_chord_style);
    ysw_sdb_add_button(sdb, "Edit Style", on_edit_style);
    ysw_sdb_add_button(sdb, "Create Style", on_create_style);
    ysw_sdb_add_button(sdb, "Apply to All", on_apply_all);
    ysw_sdb_add_button(sdb, "Apply Selected", on_apply_selected);

    ysw_heap_free(chord_styles); // choice uses ddlist, which uses label, which allocs space
}

