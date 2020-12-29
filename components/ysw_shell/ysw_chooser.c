// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_chooser.h"
#include "ysw_app.h"
#include "ysw_array.h"
#include "ysw_common.h"
#include "ysw_edit_pane.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_style.h"
#include "zm_music.h"
#include "lvgl.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_CHOOSER"

// TODO: make this generic, so it can handle any object type (e.g. via a data provider interface)

typedef enum {
    SORT_BY_NAME,
    SORT_BY_SIZE,
    SORT_BY_AGE,
} ysw_chooser_sort_t;

typedef struct {
    ysw_bus_t *bus;
    zm_music_t *music;
    void *context;
    ysw_menu_t *menu;
    lv_obj_t *page;
    lv_obj_t *table;
    int current_row;
    QueueHandle_t queue;
    ysw_app_control_t control;
} ysw_chooser_t;

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

static void hide_selection(ysw_chooser_t *chooser)
{
    assert(chooser);
    if (chooser->current_row != -1) {
        for (int i = 0; i < 3; i++) {
            lv_table_set_cell_type(chooser->table, chooser->current_row + 1, i, NORMAL_CELL);
        }
        chooser->current_row = -1;
    }
}

static void show_selection(ysw_chooser_t *chooser, zm_section_x new_row)
{
    assert(chooser);
    assert(new_row < lv_table_get_row_cnt(chooser->table));
    for (int i = 0; i < 3; i++) {
        lv_table_set_cell_type(chooser->table, new_row + 1, i, SELECTED_CELL);
    }
    chooser->current_row = new_row;
}

static void select_row(ysw_chooser_t *chooser, zm_section_x new_row)
{
    assert(chooser);
    zm_section_x row_count = ysw_array_get_count(chooser->music->sections);
    if (row_count) {
        if (new_row >= row_count) {
            new_row = row_count - 1;
        }
        if (chooser->current_row != new_row) {
            hide_selection(chooser);
            show_selection(chooser, new_row);
            ensure_current_row_visible(chooser);
            lv_obj_invalidate(chooser->table);
        }
    }
}

static int32_t update_sections(ysw_chooser_t *chooser, zm_section_t *target)
{
    assert(chooser);
    int32_t row = -1;
    zm_section_x section_count = ysw_array_get_count(chooser->music->sections);
    lv_table_set_row_cnt(chooser->table, section_count);
    for (zm_section_x i = 0, data_row = 1; i < section_count; i++, data_row++) {
        char buffer[32];
        zm_section_t *section = ysw_array_get(chooser->music->sections, i);
        zm_step_x step_count = ysw_array_get_count(section->steps);
        zm_time_x age = chooser->music->settings.clock - section->tlm;
        lv_table_set_cell_align(chooser->table, data_row, 0, LV_LABEL_ALIGN_CENTER);
        lv_table_set_cell_align(chooser->table, data_row, 1, LV_LABEL_ALIGN_CENTER);
        lv_table_set_cell_align(chooser->table, data_row, 2, LV_LABEL_ALIGN_CENTER);
        lv_table_set_cell_value(chooser->table, data_row, 0, section->name);
        lv_table_set_cell_value(chooser->table, data_row, 1, ysw_itoa(step_count, buffer, sizeof(buffer)));
        lv_table_set_cell_value(chooser->table, data_row, 2, ysw_itoa(age, buffer, sizeof(buffer)));
        if (section == target) {
            row = i;
        }
    }
    return row;
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

zm_section_t *get_section(ysw_chooser_t *chooser)
{
    zm_section_t *section = NULL;
    zm_section_x section_count = ysw_array_get_count(chooser->music->sections);
    if (chooser->current_row >= 0 && chooser->current_row < section_count) {
        section = ysw_array_get(chooser->music->sections, chooser->current_row);
    }
    return section;
}

static void set_terminate_flag(ysw_chooser_t *chooser)
{
    chooser->control = YSW_APP_TERMINATE;
}

static void select_item(ysw_chooser_t *chooser)
{
    zm_section_t *section = get_section(chooser);
    set_terminate_flag(chooser);
    if (section) {
        ysw_event_fire_chooser_select(chooser->bus, section, chooser->context);
    }
}

static void on_close(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_chooser_t *chooser = menu->context;
    set_terminate_flag(chooser);
}

static void on_chooser_up(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_chooser_t *chooser = menu->context;
    if (chooser->current_row > 0) {
        select_row(chooser, chooser->current_row - 1);
    }
}

static void on_chooser_down(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_chooser_t *chooser = menu->context;
    zm_section_x section_count = ysw_array_get_count(chooser->music->sections);
    if (chooser->current_row + 1 < section_count) {
        select_row(chooser, chooser->current_row + 1);
    }
}

static void on_chooser_menu_select(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_chooser_t *chooser = menu->context;
    select_item(chooser);
}

static void on_chooser_sort(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_chooser_t *chooser = menu->context;
    ysw_chooser_sort_t type = item->value;
    ysw_array_comparator comparator;
    zm_section_t *section = get_section(chooser);
    switch (type) {
        default:
        case SORT_BY_NAME:
            comparator = compare_sections_by_name;
            break;
        case SORT_BY_SIZE:
            comparator = compare_sections_by_size;
            break;
        case SORT_BY_AGE:
            comparator = compare_sections_by_age;
            break;
    }
    ysw_array_sort(chooser->music->sections, comparator);
    hide_selection(chooser);
    zm_section_x section_x = update_sections(chooser, section);
    show_selection(chooser, section_x);
}

static ysw_app_control_t process_event(void *context, ysw_event_t *event)
{
    ysw_chooser_t *chooser = context;
    switch (event->header.type) {
        case YSW_EVENT_KEY_DOWN:
            ysw_menu_on_key_down(chooser->menu, event);
            break;
        case YSW_EVENT_KEY_UP:
            ysw_menu_on_key_up(chooser->menu, event);
            break;
        case YSW_EVENT_KEY_PRESSED:
            ysw_menu_on_key_pressed(chooser->menu, event);
            break;
        default:
            break;
    }
    return chooser->control;
}

static void on_chooser_page_event(struct _lv_obj_t *table, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        ysw_chooser_t *chooser = lv_obj_get_user_data(table);
        ysw_menu_show(chooser->menu);
    }
}

static void on_chooser_table_event(struct _lv_obj_t *table, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        uint16_t row = 0;
        uint16_t column = 0;
        ysw_chooser_t *chooser = lv_obj_get_user_data(table);
        if (lv_table_get_pressed_cell(table, &row, &column)) {
            if (row > 0) {
                zm_section_x new_row = row - 1; // -1 for header
                if (new_row == chooser->current_row) {
                    select_item(chooser);
                } else {
                    select_row(chooser, row - 1);
                }
            } else {
                ysw_menu_show(chooser->menu);
            }
        }
    }
}

static const ysw_menu_item_t chooser_menu[] = {
    { YSW_R1_C1, "Select", YSW_MF_COMMAND, on_chooser_menu_select, SORT_BY_NAME, NULL },
    { YSW_R1_C4, "Up", YSW_MF_COMMAND, on_chooser_up, 0, NULL },

    { YSW_R2_C1, "Sort by\nName", YSW_MF_COMMAND, on_chooser_sort, SORT_BY_NAME, NULL },
    { YSW_R2_C2, "Sort by\nSize", YSW_MF_COMMAND, on_chooser_sort, SORT_BY_SIZE, NULL },
    { YSW_R2_C3, "Sort by\nAge", YSW_MF_COMMAND, on_chooser_sort, SORT_BY_AGE, NULL },
    { YSW_R2_C4, "Down", YSW_MF_COMMAND, on_chooser_down, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, on_close, 0, NULL },
    { YSW_R4_C3, " ", YSW_MF_MINUS, ysw_menu_nop, 0, NULL }, // Keyboard (#) Menu Activation

    { 0, "Chooser", YSW_MF_END, NULL, 0, NULL },
};

void ysw_chooser_create(ysw_bus_t *bus, zm_music_t *music, zm_section_x row, void *context)
{
    assert(music);

    ysw_chooser_t *chooser = ysw_heap_allocate(sizeof(ysw_chooser_t));
    chooser->bus = bus;
    chooser->music = music;
    chooser->context = context;
    chooser->current_row = -1;
    chooser->page = lv_page_create(lv_scr_act(), NULL);
    lv_obj_set_size(chooser->page, 320, 240);
    lv_obj_align(chooser->page, NULL, LV_ALIGN_CENTER, 0, 0);

    chooser->table = lv_table_create(chooser->page, NULL);

    ysw_style_chooser(chooser->page, chooser->table);

    zm_section_x section_count = ysw_array_get_count(music->sections);
    lv_table_set_col_cnt(chooser->table, 3);
    lv_table_set_row_cnt(chooser->table, 1 + section_count);

    lv_table_set_col_width(chooser->table, 0, 160);
    lv_table_set_col_width(chooser->table, 1, 80);
    lv_table_set_col_width(chooser->table, 2, 80);

    lv_table_set_cell_value(chooser->table, 0, 0, "Section");
    lv_table_set_cell_type(chooser->table, 0, 0, HEADING_CELL);
    lv_table_set_cell_align(chooser->table, 0, 0, LV_LABEL_ALIGN_CENTER);

    lv_table_set_cell_value(chooser->table, 0, 1, "Size");
    lv_table_set_cell_type(chooser->table, 0, 1, HEADING_CELL);
    lv_table_set_cell_align(chooser->table, 0, 1, LV_LABEL_ALIGN_CENTER);

    lv_table_set_cell_value(chooser->table, 0, 2, "Age");
    lv_table_set_cell_type(chooser->table, 0, 2, HEADING_CELL);
    lv_table_set_cell_align(chooser->table, 0, 2, LV_LABEL_ALIGN_CENTER);

    lv_obj_set_user_data(chooser->page, chooser); // for tables that don't take entire page
    lv_obj_set_event_cb(chooser->page, on_chooser_page_event);
    lv_obj_set_click(chooser->page, true);

    lv_obj_set_user_data(chooser->table, chooser);
    lv_obj_set_event_cb(chooser->table, on_chooser_table_event);
    lv_obj_set_click(chooser->table, true);
    lv_obj_set_drag(chooser->table, true);
    lv_obj_set_drag_dir(chooser->table, LV_DRAG_DIR_VER);

    update_sections(chooser, NULL);
    select_row(chooser, row);

    chooser->menu = ysw_menu_create(bus, chooser_menu, ysw_app_softkey_map, chooser);

    chooser->queue = ysw_app_create_queue();
    ysw_bus_subscribe(bus, YSW_ORIGIN_KEYBOARD, chooser->queue);
    ysw_bus_subscribe(bus, YSW_ORIGIN_CHOOSER, chooser->queue); // hack to terminate ysw_app_handle_events

    ysw_app_handle_events(chooser->queue, process_event, chooser);

    ysw_bus_unsubscribe(bus, YSW_ORIGIN_KEYBOARD, chooser->queue);
    ysw_bus_unsubscribe(bus, YSW_ORIGIN_CHOOSER, chooser->queue);
    ysw_bus_delete_queue(bus, chooser->queue);

    ysw_menu_free(chooser->menu);
    lv_obj_del(chooser->page);
    ysw_heap_free(chooser);
}

