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

void ysw_chooser_hide_selection(ysw_chooser_t *chooser)
{
    assert(chooser);
    if (chooser->current_row != -1) {
        for (int i = 0; i < 3; i++) {
            lv_table_set_cell_type(chooser->table, chooser->current_row + 1, i, NORMAL_CELL);
        }
        chooser->current_row = -1;
    }
}

void ysw_chooser_show_selection(ysw_chooser_t *chooser, zm_section_x new_row)
{
    assert(chooser);
    assert(new_row < lv_table_get_row_cnt(chooser->table));
    for (int i = 0; i < 3; i++) {
        lv_table_set_cell_type(chooser->table, new_row + 1, i, SELECTED_CELL);
    }
    chooser->current_row = new_row;
}

void ysw_chooser_select_row(ysw_chooser_t *chooser, zm_section_x new_row)
{
    assert(chooser);
    assert(new_row < lv_table_get_row_cnt(chooser->table));
    if (chooser->current_row != new_row) {
        ysw_chooser_hide_selection(chooser);
        ysw_chooser_show_selection(chooser, new_row);
        ensure_current_row_visible(chooser);
        lv_obj_invalidate(chooser->table);
    }
}

void ysw_chooser_update_sections(ysw_chooser_t *chooser)
{
    assert(chooser);
    zm_section_x section_count = ysw_array_get_count(chooser->music->sections);
    for (zm_section_x i = 0, data_row = 1; i < section_count; i++, data_row++) {
        char buffer[32];
        zm_section_t *section = ysw_array_get(chooser->music->sections, i);
        zm_step_x step_count = ysw_array_get_count(section->steps);
        zm_time_x age = chooser->music->settings.clock - section->tlm;
        lv_table_set_cell_value(chooser->table, data_row, 0, section->name);
        lv_table_set_cell_value(chooser->table, data_row, 1, ysw_itoa(step_count, buffer, sizeof(buffer)));
        lv_table_set_cell_value(chooser->table, data_row, 2, ysw_itoa(age, buffer, sizeof(buffer)));
    }
}

static int compare_sections_by_name(const void *left, const void *right)
{
    const zm_section_t *left_section = *(zm_section_t * const *)left;
    const zm_section_t *right_section = *(zm_section_t * const *)right;
    return strcmp(left_section->name, right_section->name);
}

static int compare_sections_by_size(const void *left, const void *right)
{
    const zm_section_t *left_section = *(zm_section_t * const *)left;
    const zm_section_t *right_section = *(zm_section_t * const *)right;
    zm_section_x left_size = ysw_array_get_count(left_section->steps);
    zm_section_x right_size = ysw_array_get_count(right_section->steps);
    return left_size - right_size;
}

static int compare_sections_by_age(const void *left, const void *right)
{
    const zm_section_t *left_section = *(zm_section_t * const *)left;
    const zm_section_t *right_section = *(zm_section_t * const *)right;
    return right_section->tlm - left_section->tlm; // age is inverse of tlm
}

void ysw_chooser_sort(ysw_chooser_t *chooser, ysw_chooser_sort_t type)
{
    ysw_array_comparator comparator;
    zm_section_t *section = ysw_chooser_get_section(chooser);
    switch (type) {
        default:
        case YSW_CHOOSER_SORT_BY_NAME:
            comparator = compare_sections_by_name;
            break;
        case YSW_CHOOSER_SORT_BY_SIZE:
            comparator = compare_sections_by_size;
            break;
        case YSW_CHOOSER_SORT_BY_AGE:
            comparator = compare_sections_by_age;
            break;
    }
    ysw_array_sort(chooser->music->sections, comparator);
    ysw_chooser_hide_selection(chooser);
    ysw_chooser_update_sections(chooser);
    zm_section_x section_x = ysw_array_find(chooser->music->sections, section);
    ysw_chooser_show_selection(chooser, section_x);
}

typedef void (*ysw_virtual_keyboard_cb_t)(void *context, const char *text);

typedef struct {
    ysw_chooser_t *context;
    lv_obj_t *container;
    lv_obj_t *keyboard;
    lv_obj_t *textarea;
    ysw_virtual_keyboard_cb_t callback;
} ysw_virtual_keyboard_t;

static void on_keyboard_event(lv_obj_t *keyboard, lv_event_t event)
{
    lv_keyboard_def_event_cb(keyboard, event);
    if (event == LV_EVENT_CANCEL) {
        ysw_virtual_keyboard_t *virtual_keyboard = lv_obj_get_user_data(keyboard);
        lv_keyboard_set_textarea(keyboard, NULL);
        lv_obj_del(virtual_keyboard->container);
        virtual_keyboard->keyboard = NULL;
    } else if (event == LV_EVENT_APPLY) {
        ysw_virtual_keyboard_t *virtual_keyboard = lv_obj_get_user_data(keyboard);
        const char *text = lv_textarea_get_text(virtual_keyboard->textarea);
        virtual_keyboard->callback(virtual_keyboard->context, text);
        lv_keyboard_set_textarea(keyboard, NULL);
        lv_obj_del(virtual_keyboard->container);
        virtual_keyboard->keyboard = NULL;
    }
}

static void create_keyboard(ysw_virtual_keyboard_t *virtual_keyboard)
{
    virtual_keyboard->keyboard = lv_keyboard_create(virtual_keyboard->container, NULL);
    lv_obj_set_user_data(virtual_keyboard->keyboard, virtual_keyboard);
    lv_keyboard_set_cursor_manage(virtual_keyboard->keyboard, true);
    lv_obj_set_event_cb(virtual_keyboard->keyboard, on_keyboard_event);
    lv_keyboard_set_textarea(virtual_keyboard->keyboard, virtual_keyboard->textarea);
}

static void on_textarea_event(lv_obj_t *textarea, lv_event_t event)
{
    ysw_virtual_keyboard_t *virtual_keyboard = lv_obj_get_user_data(textarea);
    if (event == LV_EVENT_CLICKED && virtual_keyboard->keyboard == NULL) {
        create_keyboard(virtual_keyboard);
    }
}

static void on_rename_complete(void *context, const char *text)
{
    ysw_chooser_t *chooser = context;
    zm_section_t *section = ysw_chooser_get_section(chooser);
    section->name = ysw_heap_strdup(text);
    ysw_chooser_update_sections(chooser);
}

void ysw_chooser_rename(ysw_chooser_t *chooser)
{
    ysw_virtual_keyboard_t *virtual_keyboard = ysw_heap_allocate(sizeof(ysw_virtual_keyboard_t));
    virtual_keyboard->context = chooser;
    virtual_keyboard->callback = on_rename_complete;
    virtual_keyboard->container = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_size(virtual_keyboard->container, 320, 240);
    lv_obj_align(virtual_keyboard->container, NULL, LV_ALIGN_CENTER, 0, 0);

    lv_obj_set_style_local_bg_color(virtual_keyboard->container, 0, 0, LV_COLOR_MAROON);
    lv_obj_set_style_local_bg_grad_color(virtual_keyboard->container, 0, 0, LV_COLOR_BLACK);
    lv_obj_set_style_local_bg_grad_dir(virtual_keyboard->container, 0, 0, LV_GRAD_DIR_VER);
    lv_obj_set_style_local_bg_opa(virtual_keyboard->container, 0, 0, LV_OPA_100);
    lv_obj_set_style_local_text_color(virtual_keyboard->container, 0, 0, LV_COLOR_YELLOW);
    lv_obj_set_style_local_text_opa(virtual_keyboard->container, 0, 0, LV_OPA_100);

    zm_section_t *section = ysw_chooser_get_section(chooser);
    virtual_keyboard->textarea  = lv_textarea_create(virtual_keyboard->container, NULL);
    lv_obj_set_user_data(virtual_keyboard->textarea, virtual_keyboard);
    lv_obj_align(virtual_keyboard->textarea, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);
    lv_obj_set_event_cb(virtual_keyboard->textarea, on_textarea_event);
    lv_textarea_set_text(virtual_keyboard->textarea, section->name);
    lv_obj_set_size(virtual_keyboard->textarea, 320, 100);

    create_keyboard(virtual_keyboard);
}

ysw_chooser_t *ysw_chooser_create(zm_music_t *music, zm_section_t *current_section)
{
    assert(music);
    assert(current_section);
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
