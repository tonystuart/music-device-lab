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
#include "ysw_csn.h"
#include "ysw_cs.h"
#include "ysw_ticks.h"
#include "ysw_lv_styles.h"

#include "stdio.h"
#include "esp_log.h"
#include "lvgl.h"

#define TAG "YSW_SDB"

static ysw_sdb_t *get_sdb(lv_obj_t *field)
{
    lv_obj_t *container = lv_obj_get_parent(field);
    lv_obj_t *page = lv_obj_get_parent(container);
    lv_obj_t *win = lv_obj_get_parent(page);
    ysw_sdb_t *sdb = lv_obj_get_user_data(win);
    return sdb;
}

static void on_sdb_event(lv_obj_t *win, lv_event_t event)
{
    if (event == LV_EVENT_DELETE) {
        ysw_sdb_t *sdb = lv_obj_get_user_data(win);
        ysw_heap_free(sdb);
    }
}

static void on_kb_event(lv_obj_t *keyboard, lv_event_t event)
{
    lv_kb_def_event_cb(keyboard, event);

    if (event == LV_EVENT_APPLY || event == LV_EVENT_CANCEL) {
        lv_obj_t *ta = lv_kb_get_ta(keyboard);
        if (ta) {
            lv_ta_set_cursor_type(ta, LV_CURSOR_LINE|LV_CURSOR_HIDDEN);
            ysw_sdb_t *sdb = get_sdb(ta);
            lv_obj_del(sdb->kb);
            sdb->kb = NULL;
        }
    }
}

static void on_ta_event(lv_obj_t *ta, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        ysw_sdb_t *sdb = get_sdb(ta);
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
    if (event == LV_EVENT_VALUE_CHANGED) {
        const char *new_value = lv_ta_get_text(ta);
        ysw_sdb_string_cb_t cb = lv_obj_get_user_data(ta);
        if (cb) {
            cb(new_value);
        }
    }
}

static void on_ddlist_event(lv_obj_t *ddlist, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        uint8_t new_value = lv_ddlist_get_selected(ddlist);
        ysw_sdb_choice_cb_t cb = lv_obj_get_user_data(ddlist);
        if (cb) {
            cb(new_value);
        }
    }
}

static void create_field_name(ysw_sdb_t *sdb, const char *name)
{
    lv_obj_t *label = lv_label_create(sdb->win, NULL);
    lv_label_set_long_mode(label, LV_LABEL_LONG_BREAK);
    lv_label_set_text(label, name);
    lv_label_set_align(label, LV_LABEL_ALIGN_RIGHT);
    lv_obj_set_width(label, 100);
}

ysw_sdb_t *ysw_sdb_create()
{
    ysw_sdb_t *sdb = ysw_heap_allocate(sizeof(ysw_sdb_t));
    sdb->win = lv_win_create(lv_scr_act(), NULL);
    lv_obj_set_user_data(sdb->win, sdb);

    lv_obj_align(sdb->win, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_win_set_style(sdb->win, LV_WIN_STYLE_BG, &lv_style_pretty);
    lv_win_set_style(sdb->win, LV_WIN_STYLE_CONTENT, &win_style_content);
    lv_win_set_title(sdb->win, "Settings");
    lv_win_set_layout(sdb->win, LV_LAYOUT_PRETTY);

    lv_obj_t *close_btn = lv_win_add_btn(sdb->win, LV_SYMBOL_CLOSE);
    lv_obj_set_event_cb(close_btn, lv_win_close_event_cb);

    lv_win_set_btn_size(sdb->win, 20);
    lv_obj_set_event_cb(sdb->win, on_sdb_event);

    return sdb;
}

void ysw_sdb_add_string(ysw_sdb_t *sdb, ysw_sdb_string_cb_t cb, const char *name, const char *value)
{
    create_field_name(sdb, name);

    lv_obj_t *ta = lv_ta_create(sdb->win, NULL);
    lv_obj_set_user_data(ta, cb);
    lv_obj_set_width(ta, 200);
    lv_obj_set_protect(ta, LV_PROTECT_FOLLOW);
    lv_ta_set_style(ta, LV_TA_STYLE_BG, &value_cell);
    lv_ta_set_one_line(ta, true);
    lv_ta_set_cursor_type(ta, LV_CURSOR_LINE|LV_CURSOR_HIDDEN);
    lv_ta_set_text(ta, value);
    lv_obj_set_event_cb(ta, on_ta_event);
}

void ysw_sdb_add_choice(ysw_sdb_t *sdb, ysw_sdb_choice_cb_t cb, const char *name, uint8_t value, const char *options)
{
    create_field_name(sdb, name);

    lv_obj_t *ddlist = lv_ddlist_create(sdb->win, NULL);
    lv_obj_set_user_data(ddlist, cb);
    lv_ddlist_set_draw_arrow(ddlist, true);
    lv_ddlist_set_options(ddlist, options);
    lv_ddlist_set_fix_height(ddlist, 100);
    lv_ddlist_set_fix_width(ddlist, 200);
    lv_ddlist_set_style(ddlist, LV_DDLIST_STYLE_BG, &value_cell);
    lv_ddlist_set_align(ddlist, LV_LABEL_ALIGN_LEFT);
    lv_ddlist_set_selected(ddlist, value);
    lv_obj_set_protect(ddlist, LV_PROTECT_FOLLOW);
    lv_obj_set_event_cb(ddlist, on_ddlist_event);
}

void ysw_sdb_close(ysw_sdb_t *sdb)
{
    lv_obj_del(sdb->win); // on_sdb_event will free sdb
}

