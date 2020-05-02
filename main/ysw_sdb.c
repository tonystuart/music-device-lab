// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_sdb.h"
#include "ysw_heap.h"
#include "ysw_chord.h"
#include "ysw_ticks.h"
#include "ysw_lv_styles.h"

#include "stdio.h"
#include "esp_log.h"
#include "lvgl.h"

#define TAG "YSW_SDB"

// TODO: Add win handler and free user data on close

static void ysw_sdb_free(ysw_sdb_t *sdb)
{
    lv_obj_t *win = sdb->win;
    lv_obj_t *page = lv_win_get_content(win);
    lv_obj_t *container = lv_page_get_scrl(page);
    lv_obj_t *child;
    LV_LL_READ(container->child_ll, child) {
        void *user_data = lv_obj_get_user_data(child);
        if (user_data) {
            ysw_heap_free(user_data);
        }
    }
    ysw_heap_free(sdb);
}

static void create_field_name(ysw_sdb_t *sdb, ysw_sdb_field_t *field)
{
    lv_obj_t *label = lv_label_create(sdb->win, NULL);
    lv_label_set_long_mode(label, LV_LABEL_LONG_BREAK);
    lv_label_set_text(label, field->name);
    lv_label_set_align(label, LV_LABEL_ALIGN_RIGHT);
    lv_obj_set_width(label, 75);
}

static void on_kb_event(lv_obj_t *keyboard, lv_event_t event)
{
    lv_kb_def_event_cb(keyboard, event);

    if (event == LV_EVENT_APPLY || event == LV_EVENT_CANCEL) {
        lv_obj_t *ta = lv_kb_get_ta(keyboard);
        if (ta) {
            lv_ta_set_cursor_type(ta, LV_CURSOR_LINE|LV_CURSOR_HIDDEN);
            ysw_sdb_string_data_t *data = lv_obj_get_user_data(ta);
            ysw_sdb_t *sdb = data->sdb;
            lv_obj_del(sdb->kb);
            sdb->kb = NULL;
        }
    }
}

static void on_ta_event(lv_obj_t *ta, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        ysw_sdb_string_data_t *data = lv_obj_get_user_data(ta);
        ysw_sdb_t *sdb = data->sdb;
        lv_obj_t *win = sdb->win;
        lv_obj_t *page = lv_win_get_content(win);
        lv_obj_t *container = lv_page_get_scrl(page);
        lv_cont_set_layout(container, LV_LAYOUT_OFF);
        if (!sdb->kb) {
            sdb->kb = lv_kb_create(win, NULL);
            lv_obj_set_width(sdb->kb, 290);
            lv_obj_set_event_cb(sdb->kb, on_kb_event);
            lv_kb_set_cursor_manage(sdb->kb, true);
        }
        if (lv_kb_get_ta(sdb->kb) == ta) {
            // click again in same field - hide the kb
            lv_ta_set_cursor_type(ta, LV_CURSOR_LINE|LV_CURSOR_HIDDEN);
            lv_obj_del(sdb->kb);
            sdb->kb = NULL;
        } else {
            lv_kb_set_ta(sdb->kb, ta);
            lv_ll_move_before(&container->child_ll, sdb->kb, ta);
        }
        lv_cont_set_layout(container, LV_LAYOUT_PRETTY);
    }
}

static void on_roller_event(lv_obj_t *roller, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        ESP_LOGD(TAG, "on_roller_event value changed, index=%d", lv_roller_get_selected(roller));
    }
}

static void create_string_data(ysw_sdb_t *sdb, lv_obj_t *ta, uint8_t index)
{
    ysw_sdb_string_data_t *data = ysw_heap_allocate(sizeof(ysw_sdb_string_data_t));
    data->sdb = sdb;
    data->index = index;
    lv_obj_set_user_data(ta, data);
}

static void create_string_value(ysw_sdb_t *sdb, ysw_sdb_field_t *field, uint8_t index)
{
    lv_obj_t *ta = lv_ta_create(sdb->win, NULL);
    lv_obj_set_width(ta, 200);
    lv_obj_set_protect(ta, LV_PROTECT_FOLLOW);
    lv_ta_set_style(ta, LV_TA_STYLE_BG, &value_cell);
    lv_ta_set_one_line(ta, true);
    lv_ta_set_cursor_type(ta, LV_CURSOR_LINE|LV_CURSOR_HIDDEN);
    lv_ta_set_text(ta, field->value.string.original);

    create_string_data(sdb, ta, index);
    lv_obj_set_event_cb(ta, on_ta_event);
}

static void create_number_value(ysw_sdb_t *sdb, ysw_sdb_field_t *field, uint8_t index)
{
}

static void create_choice_data(ysw_sdb_t *sdb, lv_obj_t *roller, uint8_t index)
{
    ysw_sdb_choice_data_t *data = ysw_heap_allocate(sizeof(ysw_sdb_choice_data_t));
    data->sdb = sdb;
    data->index = index;
    lv_obj_set_user_data(roller, data);
}

static void create_choice_value(ysw_sdb_t *sdb, ysw_sdb_field_t *field, uint8_t index)
{
    lv_obj_t *roller = lv_roller_create(sdb->win, NULL);
    lv_roller_set_options(roller, field->value.choice.options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(roller, 4);
    //lv_obj_set_width(roller, 200);
    lv_roller_set_fix_width(roller, 200);
    lv_obj_set_protect(roller, LV_PROTECT_FOLLOW);
    lv_roller_set_style(roller, LV_ROLLER_STYLE_BG, &value_cell);
    lv_roller_set_align(roller, LV_LABEL_ALIGN_LEFT);
    lv_roller_set_selected(roller, field->value.choice.original, LV_ANIM_OFF);

    create_choice_data(sdb, roller, index);
    lv_obj_set_event_cb(roller, on_roller_event);
}

ysw_sdb_t *ysw_sdb_create(ysw_cs_t *cs, ysw_sdb_field_t *fields, uint8_t field_count)
{
    ysw_sdb_t *sdb = ysw_heap_allocate(sizeof(ysw_sdb_t));
    sdb->win = lv_win_create(lv_scr_act(), NULL);

    lv_obj_align(sdb->win, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_win_set_style(sdb->win, LV_WIN_STYLE_BG, &lv_style_pretty);
    lv_win_set_style(sdb->win, LV_WIN_STYLE_CONTENT, &win_style_content);
    lv_win_set_title(sdb->win, "Settings");
    lv_win_set_layout(sdb->win, LV_LAYOUT_OFF);

    lv_obj_t *close_btn = lv_win_add_btn(sdb->win, LV_SYMBOL_CLOSE);
    lv_obj_set_event_cb(close_btn, lv_win_close_event_cb);

    lv_win_add_btn(sdb->win, LV_SYMBOL_OK);
    lv_win_set_btn_size(sdb->win, 20);

    for (uint8_t i = 0; i < field_count; i++) {
        ysw_sdb_field_t *field = &fields[i];
        switch (field->type) {
            case YSW_SDB_STRING:
                create_field_name(sdb, field);
                create_string_value(sdb, field, i);
                break;
            case YSW_SDB_NUMBER:
                create_field_name(sdb, field);
                create_number_value(sdb, field, i);
                break;
            case YSW_SDB_CHOICE:
                create_field_name(sdb, field);
                create_choice_value(sdb, field, i);
                break;
        }
    }

    lv_win_set_layout(sdb->win, LV_LAYOUT_PRETTY);
    return sdb;
}

void ysw_sdb_close(ysw_sdb_t *sdb)
{
    ysw_sdb_free(sdb);
    lv_obj_del(sdb->win);
}
