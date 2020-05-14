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

static lv_signal_cb_t super_signal_cb;

static const char *headings[] = {
    "Measure", "Degree", "Chord Style"
};

static const uint8_t widths[] = {
    79, 79, 158
};

#define COLUMN_COUNT (sizeof(headings) / sizeof(char*))

typedef struct {
    int16_t row;
    int16_t column;
    int16_t old_step_index;
    bool is_cell_edit;
} selection_t;

static selection_t selection = {
    .row = -1,
    .old_step_index = -1
};

// TODO: Use ysw_cpe_t or lv_obj_t relationships instead of globals
static lv_obj_t *table;

static uint8_t get_row(lv_coord_t y)
{
    return y / 30;
}

static uint8_t get_column(lv_coord_t x)
{
    lv_coord_t width = 0;
    for (uint8_t i = 0; i < COLUMN_COUNT; i++) {
        width += widths[i];
        if (x < width) {
            return i;
        }
    }
    return COLUMN_COUNT - 1;
}

static void clear_selected_step_highlight()
{
    if (selection.old_step_index != -1) {
        for (int i = 0; i < COLUMN_COUNT; i++) {
            lv_table_set_cell_type(table, selection.old_step_index + 1, i, 1); // +1 for heading
        }
    }
    selection.old_step_index = -1;
    lv_obj_refresh_style(table);
}

static void select_step(uint8_t step_index)
{
    clear_selected_step_highlight();
    for (int i = 0; i < COLUMN_COUNT; i++) {
        lv_table_set_cell_type(table, step_index + 1, i, 4); // +1 for heading
    }
    selection.old_step_index = step_index;
    lv_obj_refresh_style(table);
}

static lv_res_t my_scrl_signal_cb(lv_obj_t *scrl, lv_signal_t sign, void *param)
{
    //ESP_LOGD(TAG, "my_scrl_signal_cb sign=%d", sign);
    if (sign == LV_SIGNAL_PRESSED) {
        lv_indev_t *indev_act = (lv_indev_t*)param;
        lv_indev_proc_t *proc = &indev_act->proc;
        lv_area_t scrl_coords;
        lv_obj_get_coords(scrl, &scrl_coords);
        uint16_t row = get_row((proc->types.pointer.act_point.y + -scrl_coords.y1));
        uint16_t column = get_column(proc->types.pointer.act_point.x);
        ESP_LOGD(TAG, "act_point x=%d, y=%d, scrl_coords x1=%d, y1=%d, x2=%d, y2=%d, row+1=%d, column+1=%d", proc->types.pointer.act_point.x, proc->types.pointer.act_point.y, scrl_coords.x1, scrl_coords.y1, scrl_coords.x2, scrl_coords.y2, row+1, column+1);

        // TODO: use a metdata structure to hold info about the cells
        if (row) {
            if (selection.is_cell_edit) {
                lv_table_set_cell_type(table, selection.row, selection.column, 1);
                lv_obj_refresh_style(table);
                selection.is_cell_edit = false;
                selection.row = -1;
            }
            uint8_t step_index = row - 1; // -1 for headings
            select_step(step_index);
            if (selection.row == row && selection.column == column) {
                lv_table_set_cell_type(table, selection.row, selection.column, 2);
                lv_obj_refresh_style(table);
                selection.is_cell_edit = true;
            } else {
                selection.row = row;
                selection.column = column;
            }
        }
    }
    return super_signal_cb(scrl, sign, param);
}

ysw_cpe_t *ysw_cpe_create(lv_obj_t *win)
{
    ysw_cpe_t *cpe = ysw_heap_allocate(sizeof(ysw_cpe_t));

    cpe->table = lv_table_create(win, NULL);
    table = cpe->table; // TODO: remove static global

    lv_obj_align(cpe->table, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

    lv_table_set_style(cpe->table, LV_TABLE_STYLE_BG, &plain_color_tight);
    lv_table_set_style(cpe->table, LV_TABLE_STYLE_CELL1, &value_cell);
    lv_table_set_style(cpe->table, LV_TABLE_STYLE_CELL2, &cell_editor_style);
    lv_table_set_style(cpe->table, LV_TABLE_STYLE_CELL3, &name_cell);
    lv_table_set_style(cpe->table, LV_TABLE_STYLE_CELL4, &selected_cell);

    lv_table_set_row_cnt(cpe->table, 1); // just the headings for now
    lv_table_set_col_cnt(cpe->table, COLUMN_COUNT);

    for (int i = 0; i < COLUMN_COUNT; i++) {
        ESP_LOGD(TAG, "setting column attributes");
        lv_table_set_cell_type(cpe->table, 0, i, 3);
        lv_table_set_cell_align(cpe->table, 0, i, LV_LABEL_ALIGN_CENTER);
        lv_table_set_cell_value(cpe->table, 0, i, headings[i]);
        lv_table_set_col_width(cpe->table, i, widths[i]);
    }

    lv_obj_t *page = lv_win_get_content(win);
    lv_obj_t *scroller = lv_page_get_scrl(page);
    super_signal_cb = lv_obj_get_signal_cb(scroller);
    lv_obj_set_signal_cb(scroller, my_scrl_signal_cb);

    return cpe;
}

void ysw_cpe_set_cp(ysw_cpe_t *cpe, ysw_cp_t *cp)
{
    clear_selected_step_highlight();

    uint32_t step_count = ysw_cp_get_step_count(cp);
    lv_table_set_row_cnt(cpe->table, step_count + 1); // +1 for the headings

    double measure = 0;
    for (uint32_t i = 0; i < step_count; i++) {
        ESP_LOGD(TAG, "setting step attributes, i=%d", i);
        char buffer[16];
        int row = i + 1; // +1 for header
        ysw_step_t *step = ysw_cp_get_step(cp, i);
        double beats_per_measure = ysw_cs_get_beats_per_measure(step->cs);
        double beat_unit = ysw_cs_get_beat_unit(step->cs);
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


