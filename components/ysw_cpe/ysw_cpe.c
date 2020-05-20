// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// Chord Progression Editor (CPE)

#include "ysw_cpe.h"

#include "ysw_cs.h"
#include "ysw_cn.h"
#include "ysw_heap.h"
#include "ysw_lv_styles.h"
#include "ysw_ticks.h"

#include "lvgl.h"

#include "esp_log.h"

#include "math.h"
#include "stdio.h"

#define TAG "YSW_CPE"

// See https://en.wikipedia.org/wiki/Chord_letters#Chord_quality

static lv_signal_cb_t prev_scrl_signal_cb;
static lv_signal_cb_t prev_ddlist_signal_cb;

typedef struct {
    const char *name;
    uint8_t width;
} headings_t;

static const headings_t headings[] = {
    { "Measure", 79 },
    { "Degree", 79 },
    { "Chord Style", 158 },
};

#define COLUMN_COUNT (sizeof(headings) / sizeof(headings_t))

static void close_cell_editor(ysw_cpe_t *cpe)
{
    if (cpe->ddlist) {
        lv_obj_del(cpe->ddlist);
        cpe->ddlist = NULL;
    }
}

static lv_res_t ddlist_signal_cb(lv_obj_t *ddlist, lv_signal_t signal, void *param)
{
    //ESP_LOGD(TAG, "ddlist_signal_cb signal=%d", signal);
    return prev_ddlist_signal_cb(ddlist, signal, param);
}

static void on_ddlist_event(lv_obj_t *ddlist, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        ysw_cpe_t *cpe = lv_obj_get_user_data(ddlist);
        if (cpe) {
            close_cell_editor(cpe);
        }
#if 0
        uint8_t new_value = lv_ddlist_get_selected(ddlist);
        ysw_sdb_choice_cb_t cb = lv_obj_get_user_data(ddlist);
        if (cb) {
            cb(new_value);
        }
#endif
    }
}

static uint8_t get_row(lv_coord_t y)
{
    return y / 30;
}

static uint16_t get_column(lv_coord_t x)
{
    lv_coord_t width = 0;
    for (uint16_t i = 0; i < COLUMN_COUNT; i++) {
        width += headings[i].width;
        if (x < width) {
            return i;
        }
    }
    return COLUMN_COUNT - 1;
}

static void set_bounds(lv_obj_t *obj, lv_coord_t row, lv_coord_t column)
{
    lv_coord_t x = 2; // TODO: use table cell border + padding
    for (uint16_t i = 0; i < column; i++) {
        x += headings[i].width;
    }
    lv_coord_t w = headings[column].width - 2; // TODO: use table cell border + padding
    lv_coord_t y = row * 30; // TODO: calculate or set row height
    lv_coord_t h = 30;
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_ddlist_set_fix_width(obj, w);
    //lv_ddlist_set_fix_height(obj, 120);
}

static void clear_highlight(ysw_cpe_t *cpe)
{
    close_cell_editor(cpe);
    if (cpe->selection.old_step_index != -1) {
        for (int i = 0; i < COLUMN_COUNT; i++) {
            lv_table_set_cell_type(cpe->table, cpe->selection.old_step_index + 1, i, 1); // +1 for heading
        }
    }
    cpe->selection.old_step_index = -1;
    lv_obj_refresh_style(cpe->table);
}

static void select_step(ysw_cpe_t *cpe, uint8_t step_index)
{
    clear_highlight(cpe);
    for (int i = 0; i < COLUMN_COUNT; i++) {
        lv_table_set_cell_type(cpe->table, step_index + 1, i, 4); // +1 for heading
    }
    cpe->selection.old_step_index = step_index;
    lv_obj_refresh_style(cpe->table);
}

static void on_click(ysw_cpe_t *cpe, uint16_t row, uint16_t column)
{
    if (row) {
        if (cpe->selection.is_cell_edit) {
            lv_table_set_cell_type(cpe->table, cpe->selection.row, cpe->selection.column, 1);
            lv_obj_refresh_style(cpe->table);
            cpe->selection.is_cell_edit = false;
            cpe->selection.row = -1;
        }
        uint8_t step_index = row - 1; // -1 for headings
        select_step(cpe, step_index);
        if (cpe->selection.row == row && cpe->selection.column == column) {
            close_cell_editor(cpe);
            lv_table_set_cell_type(cpe->table, cpe->selection.row, cpe->selection.column, 2);
            lv_obj_refresh_style(cpe->table);
            ysw_step_t *step = ysw_cp_get_step(cpe->cp, step_index);
            cpe->selection.is_cell_edit = true;
            cpe->ddlist = lv_ddlist_create(cpe->table, NULL);
            set_bounds(cpe->ddlist, row, column);
            lv_ddlist_set_draw_arrow(cpe->ddlist, true);
            lv_ddlist_set_style(cpe->ddlist, LV_DDLIST_STYLE_BG, &lv_style_transp);
            lv_ddlist_set_options(cpe->ddlist, "I\nII\nIII\nIV\nV\nVI\nVII");
            lv_ddlist_set_selected(cpe->ddlist, step->degree - 1); // -1 because degrees are 1-based
            lv_ddlist_set_align(cpe->ddlist, LV_LABEL_ALIGN_CENTER);
            //lv_ddlist_open(cpe->ddlist, LV_ANIM_ON);
            lv_obj_set_user_data(cpe->ddlist, cpe);
            lv_obj_set_event_cb(cpe->ddlist, on_ddlist_event);
            // TODO: remove if we don't need to handle any signals
            if (!prev_ddlist_signal_cb) {
                prev_ddlist_signal_cb = lv_obj_get_signal_cb(cpe->ddlist);
            }
            lv_obj_set_signal_cb(cpe->ddlist, ddlist_signal_cb);
        } else {
            cpe->selection.row = row;
            cpe->selection.column = column;
        }
    }
}

static lv_res_t scrl_signal_cb(lv_obj_t *scrl, lv_signal_t signal, void *param)
{
    //ESP_LOGD(TAG, "scrl_signal_cb signal=%d", signal);
    ysw_cpe_t *cpe = lv_obj_get_user_data(scrl);
    if (signal == LV_SIGNAL_PRESSED) {
        lv_indev_t *indev_act = (lv_indev_t*)param;
        lv_indev_proc_t *proc = &indev_act->proc;
        lv_area_t scrl_coords;
        lv_obj_get_coords(scrl, &scrl_coords);
        uint16_t row = get_row((proc->types.pointer.act_point.y + -scrl_coords.y1));
        uint16_t column = get_column(proc->types.pointer.act_point.x);
        ESP_LOGD(TAG, "act_point x=%d, y=%d, scrl_coords x1=%d, y1=%d, x2=%d, y2=%d, row+1=%d, column+1=%d", proc->types.pointer.act_point.x, proc->types.pointer.act_point.y, scrl_coords.x1, scrl_coords.y1, scrl_coords.x2, scrl_coords.y2, row+1, column+1);
        on_click(cpe, row, column);
    } else if (signal == LV_EVENT_SHORT_CLICKED) {
        ESP_LOGD(TAG, "scrol_signal-cb LV_EVENT_SHORT_CLICKED");
    }
    return prev_scrl_signal_cb(scrl, signal, param);
}

ysw_cpe_t *ysw_cpe_create(lv_obj_t *win)
{
    ysw_cpe_t *cpe = ysw_heap_allocate(sizeof(ysw_cpe_t));
    cpe->selection.row = -1;
    cpe->selection.old_step_index = -1;

    cpe->table = lv_table_create(win, NULL);
    lv_obj_set_user_data(cpe->table, cpe);

    lv_obj_align(cpe->table, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

    lv_table_set_style(cpe->table, LV_TABLE_STYLE_BG, &ysw_style_plain_color_tight);
    lv_table_set_style(cpe->table, LV_TABLE_STYLE_CELL1, &ysw_style_white_cell);
    lv_table_set_style(cpe->table, LV_TABLE_STYLE_CELL2, &ysw_style_red_cell);
    lv_table_set_style(cpe->table, LV_TABLE_STYLE_CELL3, &ysw_style_gray_cell);
    lv_table_set_style(cpe->table, LV_TABLE_STYLE_CELL4, &ysw_style_yellow_cell);

    lv_table_set_row_cnt(cpe->table, 1); // just the headings for now
    lv_table_set_col_cnt(cpe->table, COLUMN_COUNT);

    for (int i = 0; i < COLUMN_COUNT; i++) {
        ESP_LOGD(TAG, "setting column attributes");
        lv_table_set_cell_type(cpe->table, 0, i, 3);
        lv_table_set_cell_align(cpe->table, 0, i, LV_LABEL_ALIGN_CENTER);
        lv_table_set_cell_value(cpe->table, 0, i, headings[i].name);
        lv_table_set_col_width(cpe->table, i, headings[i].width);
    }

    lv_obj_t *page = lv_win_get_content(win);
    lv_obj_t *scrl = lv_page_get_scrl(page);
    lv_obj_set_user_data(scrl, cpe);
    if (!prev_scrl_signal_cb) {
        prev_scrl_signal_cb = lv_obj_get_signal_cb(scrl);
    }
    lv_obj_set_signal_cb(scrl, scrl_signal_cb);

    return cpe;
}

void ysw_cpe_set_cp(ysw_cpe_t *cpe, ysw_cp_t *cp)
{
    cpe->cp = cp;

    clear_highlight(cpe);

    uint32_t step_count = ysw_cp_get_step_count(cp);
    lv_table_set_row_cnt(cpe->table, step_count + 1); // +1 for the headings

    double measure = 0;
    for (uint32_t i = 0; i < step_count; i++) {
        ESP_LOGD(TAG, "setting step attributes, i=%d", i);
        char buffer[16];
        int row = i + 1; // +1 for header
        ysw_step_t *step = ysw_cp_get_step(cp, i);
        double beats_per_measure = step->cs->divisions; // ysw_cs_get_beats_per_measure(step->cs);
        double beat_unit = 4; // ysw_cs_get_beat_unit(step->cs);
        measure += beats_per_measure / beat_unit;
        lv_table_set_cell_value(cpe->table, row, 0, ysw_itoa(measure, buffer, sizeof(buffer)));
        lv_table_set_cell_value(cpe->table, row, 1, ysw_degree_get_name(step->degree));
        lv_table_set_cell_value(cpe->table, row, 2, step->cs->name);
        lv_table_set_cell_crop(cpe->table, row, 2, true);
        for (int j = 0; j < COLUMN_COUNT; j++) {
            lv_table_set_cell_align(cpe->table, row, j, LV_LABEL_ALIGN_CENTER);
        }
    }
}

void ysw_cpe_set_event_cb(ysw_cpe_t *cpe, ysw_cpe_event_cb_t event_cb)
{
}


