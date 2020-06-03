// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_csl.h"

#include "ysw_cs.h"
#include "ysw_sn.h"
#include "ysw_heap.h"
#include "ysw_lv_styles.h"

#include "lvgl.h"

#include "esp_log.h"

#include "math.h"
#include "stdio.h"

#define TAG "YSW_CSL"

// See https://en.wikipedia.org/wiki/Chord_letters#Chord_quality

static lv_signal_cb_t prev_scrl_signal_cb;

typedef struct {
    const char *name;
    uint8_t width;
} headings_t;

static const headings_t headings[] = {
    { "Chord Style", 158 },
    { "Divisons", 79 },
    { "Notes", 79 },
};

#define COLUMN_COUNT (sizeof(headings) / sizeof(headings_t))

static uint8_t get_row(lv_coord_t y)
{
    return y / 30;
}

static void clear_highlight(ysw_csl_t *csl)
{
    if (csl->selection.cs_index != -1) {
        for (int i = 0; i < COLUMN_COUNT; i++) {
            lv_table_set_cell_type(csl->table, csl->selection.cs_index + 1, i, 1); // +1 for heading
        }
    }
    csl->selection.cs_index = -1;
    lv_obj_refresh_style(csl->table);
}

static void select_ps(ysw_csl_t *csl, uint8_t ps_index)
{
    clear_highlight(csl);
    for (int i = 0; i < COLUMN_COUNT; i++) {
        lv_table_set_cell_type(csl->table, ps_index + 1, i, 4); // +1 for heading
    }
    csl->selection.cs_index = ps_index;
    lv_obj_refresh_style(csl->table);
}

static void on_click(ysw_csl_t *csl, uint16_t row)
{
    if (row) {
        uint8_t ps_index = row - 1; // -1 for headings
        select_ps(csl, ps_index);
        csl->selection.row = row;
    }
}

static lv_res_t scrl_signal_cb(lv_obj_t *scrl, lv_signal_t signal, void *param)
{
    //ESP_LOGD(TAG, "scrl_signal_cb signal=%d", signal);
    ysw_csl_t *csl = lv_obj_get_user_data(scrl);
    if (signal == LV_SIGNAL_PRESSED) {
        lv_indev_t *indev_act = (lv_indev_t*)param;
        lv_indev_proc_t *proc = &indev_act->proc;
        lv_area_t scrl_coords;
        lv_obj_get_coords(scrl, &scrl_coords);
        uint16_t row = get_row((proc->types.pointer.act_point.y + -scrl_coords.y1));
        ESP_LOGD(TAG, "act_point x=%d, y=%d, scrl_coords x1=%d, y1=%d, x2=%d, y2=%d, row+1=%d", proc->types.pointer.act_point.x, proc->types.pointer.act_point.y, scrl_coords.x1, scrl_coords.y1, scrl_coords.x2, scrl_coords.y2, row+1);
        on_click(csl, row);
    } else if (signal == LV_EVENT_SHORT_CLICKED) {
        ESP_LOGD(TAG, "scrl_signal_cb LV_EVENT_SHORT_CLICKED");
    }
    return prev_scrl_signal_cb(scrl, signal, param);
}

static void on_settings(ysw_csl_t *csl, lv_obj_t * btn) {}
static void on_save(ysw_csl_t *csl, lv_obj_t * btn) {}
static void on_new(ysw_csl_t *csl, lv_obj_t * btn) {}
static void on_copy(ysw_csl_t *csl, lv_obj_t * btn) {}
static void on_paste(ysw_csl_t *csl, lv_obj_t * btn) {}
static void on_trash(ysw_csl_t *csl, lv_obj_t * btn) {}
static void on_sort(ysw_csl_t *csl, lv_obj_t * btn) {}
static void on_up(ysw_csl_t *csl, lv_obj_t * btn) {}
static void on_down(ysw_csl_t *csl, lv_obj_t * btn) {}
static void on_close(ysw_csl_t *csl, lv_obj_t * btn) {}
static void on_next(ysw_csl_t *csl, lv_obj_t * btn) {}
static void on_loop(ysw_csl_t *csl, lv_obj_t * btn) {}
static void on_stop(ysw_csl_t *csl, lv_obj_t * btn) {}
static void on_play(ysw_csl_t *csl, lv_obj_t * btn) {}
static void on_prev(ysw_csl_t *csl, lv_obj_t * btn) {}

static ysw_frame_t *create_frame(ysw_csl_t *csl)
{   
    ysw_frame_t *frame = ysw_frame_create(csl);

    ysw_frame_add_footer_button(frame, LV_SYMBOL_SETTINGS, on_settings);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_SAVE, on_save);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_AUDIO, on_new);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_COPY, on_copy);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_PASTE, on_paste);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_TRASH, on_trash);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_GPS, on_sort); // TODO: add SORT symbol
    ysw_frame_add_footer_button(frame, LV_SYMBOL_UP, on_up);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_DOWN, on_down);

    ysw_frame_add_header_button(frame, LV_SYMBOL_CLOSE, on_close);
    ysw_frame_add_header_button(frame, LV_SYMBOL_NEXT, on_next);
    ysw_frame_add_header_button(frame, LV_SYMBOL_LOOP, on_loop);
    ysw_frame_add_header_button(frame, LV_SYMBOL_STOP, on_stop);
    ysw_frame_add_header_button(frame, LV_SYMBOL_PLAY, on_play);
    ysw_frame_add_header_button(frame, LV_SYMBOL_PREV, on_prev);

    return frame;
}

ysw_csl_t *ysw_csl_create(ysw_music_t *music)
{
    ysw_csl_t *csl = ysw_heap_allocate(sizeof(ysw_csl_t)); // TODO: Free this!
    csl->selection.row = -1;
    csl->music = music;
    csl->frame = create_frame(csl);

    csl->table = lv_table_create(csl->frame->win, NULL);
    lv_obj_set_user_data(csl->table, csl);

    lv_obj_align(csl->table, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

    lv_table_set_style(csl->table, LV_TABLE_STYLE_BG, &ysw_style_plain_color_tight);
    lv_table_set_style(csl->table, LV_TABLE_STYLE_CELL1, &ysw_style_white_cell);
    lv_table_set_style(csl->table, LV_TABLE_STYLE_CELL2, &ysw_style_red_cell);
    lv_table_set_style(csl->table, LV_TABLE_STYLE_CELL3, &ysw_style_gray_cell);
    lv_table_set_style(csl->table, LV_TABLE_STYLE_CELL4, &ysw_style_yellow_cell);

    uint32_t cs_count = ysw_music_get_cs_count(csl->music);
    lv_table_set_row_cnt(csl->table, cs_count + 1); // +1 for the headings
    lv_table_set_col_cnt(csl->table, COLUMN_COUNT);

    for (int i = 0; i < COLUMN_COUNT; i++) {
        ESP_LOGD(TAG, "setting column attributes");
        lv_table_set_cell_type(csl->table, 0, i, 3);
        lv_table_set_cell_align(csl->table, 0, i, LV_LABEL_ALIGN_CENTER);
        lv_table_set_cell_value(csl->table, 0, i, headings[i].name);
        lv_table_set_col_width(csl->table, i, headings[i].width);
    }

    for (uint32_t i = 0; i < cs_count; i++) {
        char buffer[16];
        int row = i + 1; // +1 for header
        ysw_cs_t *cs = ysw_music_get_cs(csl->music, i);
        uint32_t sn_count = ysw_cs_get_sn_count(cs);
        lv_table_set_cell_crop(csl->table, row, 0, true);
        lv_table_set_cell_value(csl->table, row, 0, cs->name);
        lv_table_set_cell_value(csl->table, row, 1, ysw_itoa(cs->divisions, buffer, sizeof(buffer)));
        lv_table_set_cell_value(csl->table, row, 2, ysw_itoa(sn_count, buffer, sizeof(buffer)));
        for (int j = 0; j < COLUMN_COUNT; j++) {
            lv_table_set_cell_align(csl->table, row, j, LV_LABEL_ALIGN_CENTER);
        }
    }
    lv_obj_t *page = lv_win_get_content(csl->frame->win);
    lv_obj_t *scrl = lv_page_get_scrl(page);
    lv_obj_set_user_data(scrl, csl);
    if (!prev_scrl_signal_cb) {
        prev_scrl_signal_cb = lv_obj_get_signal_cb(scrl);
    }
    lv_obj_set_signal_cb(scrl, scrl_signal_cb);

    return csl;
}

