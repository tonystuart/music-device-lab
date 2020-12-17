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
#include "ysw_style.h"
#include "zm_music.h"
#include "lvgl.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_CHOOSER"

static void ensure_current_row_visible(ysw_chooser_t *chooser)
{
    lv_table_ext_t *ext = lv_obj_get_ext_attr(chooser->table);
    if (chooser->current_row < ext->row_cnt) {
        lv_coord_t h = 0;
        for (int i = 0; i < chooser->current_row; i++) {
            h += ext->row_h[i];
        }
        lv_page_scroll_ver(chooser->page, 120 - h);
    }
}

void ysw_chooser_select_row(ysw_chooser_t *chooser, zm_section_x new_row)
{
    if (chooser->current_row != new_row) {
        for (int i = 0; i < 3; i++) {
            if (chooser->current_row != -1) {
                lv_table_set_cell_type(chooser->table, chooser->current_row + 1, i, DATA_CELL);
            }
            lv_table_set_cell_type(chooser->table, new_row + 1, i, SELECTED_CELL);
        }
        chooser->current_row = new_row;
        ensure_current_row_visible(chooser);
        lv_obj_invalidate(chooser->table);
    }
}

ysw_chooser_t *ysw_chooser_create(zm_music_t *music, zm_section_t *current_section)
{
    lv_obj_t *page = lv_page_create(lv_scr_act(), NULL);
    lv_obj_set_size(page, 320, 240);
    lv_obj_align(page, NULL, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *table = lv_table_create(page, NULL);

    ysw_style_chooser(page, table);

    zm_section_x section_count = ysw_array_get_count(music->sections);
    lv_table_set_col_cnt(table, 3);
    lv_table_set_row_cnt(table, 1 + section_count);

    lv_table_set_col_width(table, 0, 160);
    lv_table_set_col_width(table, 1, 80);
    lv_table_set_col_width(table, 2, 80);

    lv_table_set_cell_value(table, 0, 0, "Section");
    lv_table_set_cell_type(table, 0, 0, HEADING_CELL);
    lv_table_set_cell_align(table, 0, 0, LV_LABEL_ALIGN_CENTER);

    lv_table_set_cell_value(table, 0, 1, "Size");
    lv_table_set_cell_type(table, 0, 1, HEADING_CELL);
    lv_table_set_cell_align(table, 0, 1, LV_LABEL_ALIGN_CENTER);

    lv_table_set_cell_value(table, 0, 2, "Age");
    lv_table_set_cell_type(table, 0, 2, HEADING_CELL);
    lv_table_set_cell_align(table, 0, 2, LV_LABEL_ALIGN_CENTER);

    int32_t new_row = -1;

    for (zm_section_x i = 0, data_row = 1; i < section_count; i++, data_row++) {
        char buffer[32];
        zm_section_t *section = ysw_array_get(music->sections, i);
        zm_step_x step_count = ysw_array_get_count(section->steps);
        zm_time_x age = music->settings.clock - section->tlm;
        lv_table_set_cell_align(table, data_row, 0, LV_LABEL_ALIGN_CENTER);
        lv_table_set_cell_align(table, data_row, 1, LV_LABEL_ALIGN_CENTER);
        lv_table_set_cell_align(table, data_row, 2, LV_LABEL_ALIGN_CENTER);
        lv_table_set_cell_value(table, data_row, 0, section->name);
        lv_table_set_cell_value(table, data_row, 1, ysw_itoa(step_count, buffer, sizeof(buffer)));
        lv_table_set_cell_value(table, data_row, 2, ysw_itoa(age, buffer, sizeof(buffer)));
        if (section == current_section) {
            new_row = i;
        }
    }

    ysw_chooser_t *chooser = ysw_heap_allocate(sizeof(ysw_chooser_t));

    chooser->music = music;
    chooser->page = page;
    chooser->table = table;
    chooser->current_row = -1;

    if (new_row != -1) {
        ysw_chooser_select_row(chooser, new_row);
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
    zm_section_x section_count = ysw_array_get_count(chooser->music->sections);
    if (chooser->current_row + 1 < section_count) {
        ysw_chooser_select_row(chooser, chooser->current_row + 1);
    }
}

zm_section_t *ysw_chooser_get_section(ysw_chooser_t *chooser)
{
    zm_section_t *section = NULL;
    zm_section_x section_count = ysw_array_get_count(chooser->music->sections);
    if (chooser->current_row >= 0 && chooser->current_row < section_count) {
        section = ysw_array_get(chooser->music->sections, chooser->current_row);
    }
    return section;
}

void ysw_chooser_delete(ysw_chooser_t *chooser)
{
    assert(chooser);
    lv_obj_del(chooser->page);
    ysw_heap_free(chooser);
}
