// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_chooser.h"
#include "ysw_array.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "zm_music.h"
#include "lvgl.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_CHOOSER"

#define DATA_CELL LV_TABLE_PART_CELL1
#define HEADING_CELL LV_TABLE_PART_CELL2
#define SELECTED_CELL LV_TABLE_PART_CELL3

void ysw_chooser_select_row(ysw_chooser_t *chooser, zm_pattern_x new_row)
{
    if (chooser->current_row != new_row) {
        for (int i = 0; i < 3; i++) {
            if (chooser->current_row != -1) {
                lv_table_set_cell_type(chooser->table, chooser->current_row + 1, i, DATA_CELL);
            }
            lv_table_set_cell_type(chooser->table, new_row + 1, i, SELECTED_CELL);
        }
        chooser->current_row = new_row;
        lv_obj_invalidate(chooser->table);
    }
}

ysw_chooser_t *ysw_chooser_create(zm_music_t *music)
{
    lv_obj_t *page = lv_page_create(lv_scr_act(), NULL);
    lv_obj_set_size(page, 320, 240);
    lv_obj_align(page, NULL, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *table = lv_table_create(page, NULL);

    lv_obj_set_style_local_bg_color(page, 0, 0, LV_COLOR_MAROON);
    lv_obj_set_style_local_bg_grad_color(page, 0, 0, LV_COLOR_BLACK);
    lv_obj_set_style_local_bg_grad_dir(page, 0, 0, LV_GRAD_DIR_VER);
    lv_obj_set_style_local_bg_opa(page, 0, 0, LV_OPA_100);
    lv_obj_set_style_local_text_opa(page, 0, 0, LV_OPA_100);

    lv_obj_set_style_local_pad_top(table, DATA_CELL, 0, 5);
    lv_obj_set_style_local_text_color(table, DATA_CELL, 0, LV_COLOR_WHITE);

    lv_obj_set_style_local_margin_bottom(table, HEADING_CELL, 0, 20);
    lv_obj_set_style_local_pad_top(table, HEADING_CELL, 0, 5);
    lv_obj_set_style_local_pad_bottom(table, HEADING_CELL, 0, 5);
    lv_obj_set_style_local_text_color(table, HEADING_CELL, 0, LV_COLOR_YELLOW);

    lv_obj_set_style_local_bg_color(table, SELECTED_CELL, 0, LV_COLOR_WHITE);
    lv_obj_set_style_local_bg_opa(table, SELECTED_CELL, 0, LV_OPA_100);
    lv_obj_set_style_local_text_color(table, SELECTED_CELL, 0, LV_COLOR_MAROON);

    zm_pattern_x pattern_count = ysw_array_get_count(music->patterns);
    lv_table_set_col_cnt(table, 3);
    lv_table_set_row_cnt(table, 1 + pattern_count);

    lv_table_set_col_width(table, 0, 160);
    lv_table_set_col_width(table, 1, 80);
    lv_table_set_col_width(table, 2, 80);

    lv_table_set_cell_value(table, 0, 0, "Name");
    lv_table_set_cell_type(table, 0, 0, HEADING_CELL);
    lv_table_set_cell_align(table, 0, 0, LV_LABEL_ALIGN_CENTER);

    lv_table_set_cell_value(table, 0, 1, "Size");
    lv_table_set_cell_type(table, 0, 1, HEADING_CELL);
    lv_table_set_cell_align(table, 0, 1, LV_LABEL_ALIGN_CENTER);

    lv_table_set_cell_value(table, 0, 2, "Age");
    lv_table_set_cell_type(table, 0, 2, HEADING_CELL);
    lv_table_set_cell_align(table, 0, 2, LV_LABEL_ALIGN_CENTER);

    for (zm_pattern_x i = 0, data_row = 1; i < pattern_count; i++, data_row++) {
        char buffer[32];
        zm_pattern_t *pattern = ysw_array_get(music->patterns, i);
        zm_division_x division_count = ysw_array_get_count(pattern->divisions);
        lv_table_set_cell_align(table, data_row, 0, LV_LABEL_ALIGN_CENTER);
        lv_table_set_cell_align(table, data_row, 1, LV_LABEL_ALIGN_CENTER);
        lv_table_set_cell_align(table, data_row, 2, LV_LABEL_ALIGN_CENTER);
        lv_table_set_cell_value(table, data_row, 0, pattern->name);
        lv_table_set_cell_value(table, data_row, 1, ysw_itoa(division_count, buffer, sizeof(buffer)));
        lv_table_set_cell_value(table, data_row, 2, ysw_itoa(pattern->age, buffer, sizeof(buffer)));
    }

    ysw_chooser_t *chooser = ysw_heap_allocate(sizeof(ysw_chooser_t));

    chooser->music = music;
    chooser->page = page;
    chooser->table = table;
    chooser->current_row = -1;

    if (pattern_count) {
        ysw_chooser_select_row(chooser, 0);
    }

    return chooser;
}

void ysw_chooser_on_up(ysw_chooser_t *chooser)
{
    if (chooser->current_row > 0) {
        ysw_chooser_select_row(chooser, chooser->current_row - 1);
    }
}

void ysw_chooser_on_down(ysw_chooser_t *chooser)
{
    zm_pattern_x pattern_count = ysw_array_get_count(chooser->music->patterns);
    if (chooser->current_row + 1 < pattern_count) {
        ysw_chooser_select_row(chooser, chooser->current_row + 1);
    }
}

zm_pattern_t *ysw_chooser_get_pattern(ysw_chooser_t *chooser)
{
    zm_pattern_t *pattern = NULL;
    zm_pattern_x pattern_count = ysw_array_get_count(chooser->music->patterns);
    if (chooser->current_row >= 0 && chooser->current_row < pattern_count) {
        pattern = ysw_array_get(chooser->music->patterns, chooser->current_row);
    }
    return pattern;
}

void ysw_chooser_delete(ysw_chooser_t *chooser)
{
    assert(chooser);
    lv_obj_del(chooser->page);
    ysw_heap_free(chooser);
}
