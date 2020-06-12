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
#include "ysw_sn.h"
#include "ysw_cs.h"
#include "ysw_ticks.h"
#include "ysw_lv_styles.h"

#include "stdio.h"
#include "esp_log.h"
#include "lvgl.h"

#define TAG "YSW_SDB"

// Key to SDB context management:
// We stash the context in the sdb
// We stash the sdb in the win's user data
// We stash the callback in each widget's user data
// We fetch the context via the sdb using the parent relationship

static lv_signal_cb_t ddlist_signal_cb;

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
    lv_keyboard_def_event_cb(keyboard, event);

    if (event == LV_EVENT_APPLY || event == LV_EVENT_CANCEL) {
        lv_obj_t *ta = lv_keyboard_get_textarea(keyboard);
        if (ta) {
            //v7.1: lv_textarea_set_cursor_type(ta, LV_CURSOR_LINE|LV_CURSOR_HIDDEN);
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
            sdb->kb = lv_keyboard_create(win, NULL);
            lv_obj_set_width(sdb->kb, 290);
            lv_obj_set_event_cb(sdb->kb, on_kb_event);
            lv_keyboard_set_cursor_manage(sdb->kb, true);
        }
        if (lv_keyboard_get_textarea(sdb->kb) == ta) {
            // click again in same field - hide the kb
            //v7.1: lv_textarea_set_cursor_type(ta, LV_CURSOR_LINE|LV_CURSOR_HIDDEN);
            lv_obj_del(sdb->kb);
            sdb->kb = NULL;
        } else {
            lv_keyboard_set_textarea(sdb->kb, ta);
            _lv_ll_move_before(&container->child_ll, sdb->kb, ta);
        }
        lv_cont_set_layout(container, LV_LAYOUT_PRETTY_MID);
    }
    if (event == LV_EVENT_VALUE_CHANGED) {
        const char *new_value = lv_textarea_get_text(ta);
        ysw_sdb_string_cb_t cb = lv_obj_get_user_data(ta);
        if (cb) {
            cb(get_sdb(ta)->context, new_value);
        }
    }
}

static void on_ddlist_event(lv_obj_t *ddlist, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        uint8_t new_value = lv_dropdown_get_selected(ddlist);
        ysw_sdb_choice_cb_t cb = lv_obj_get_user_data(ddlist);
        if (cb) {
            cb(get_sdb(ddlist)->context, new_value);
        }
    }
}

static void on_sw_event(lv_obj_t *sw, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        bool new_value = lv_switch_get_state(sw);
        ysw_sdb_switch_cb_t cb = lv_obj_get_user_data(sw);
        if (cb) {
            cb(get_sdb(sw)->context, new_value);
        }
    }
}

static void on_cb_event(lv_obj_t *cb, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        bool new_value = lv_checkbox_is_checked(cb);
        ysw_sdb_checkbox_cb_t callback = lv_obj_get_user_data(cb);
        if (callback) {
            callback(get_sdb(cb)->context, new_value);
        }
    }
}

static void on_btn_event(lv_obj_t *btn, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        ysw_sdb_button_cb_t cb = lv_obj_get_user_data(btn);
        if (cb) {
            cb(get_sdb(btn)->context);
        }
    }
}

static lv_res_t on_ddlist_signal(lv_obj_t *ddlist, lv_signal_t signal, void *param)
{
    //v7: TODO: Consider whether this is necessary with new dropdown
    lv_res_t res = ddlist_signal_cb(ddlist, signal, param);
    //ESP_LOGD(TAG, "on_ddlist_signal signal=%d", signal);
    if (res == LV_RES_OK && signal == LV_SIGNAL_COORD_CHG) {
        lv_area_t *original = (lv_area_t*)param;
        if (lv_area_get_height(original) != lv_area_get_height(&ddlist->coords)) {
            lv_obj_t *scrl = lv_obj_get_parent(ddlist);
            lv_obj_t *page = lv_obj_get_parent(scrl);
            lv_coord_t viewport_h = page->coords.y2 - page->coords.y1;
            lv_coord_t scroll_top = scrl->coords.y1 - page->coords.y1;
            lv_coord_t last_visible_y = viewport_h - scroll_top; // minus a minus
            lv_coord_t dist = ddlist->coords.y2 - last_visible_y;
            if (dist > 0) {
                lv_page_scroll_ver(page, -dist);
            }
        }
    }
    return res;
}

static void create_field_name(ysw_sdb_t *sdb, const char *name)
{
    lv_obj_t *label = lv_label_create(sdb->win, NULL);
    lv_label_set_long_mode(label, LV_LABEL_LONG_BREAK);
    lv_label_set_text(label, name);
    lv_label_set_align(label, LV_LABEL_ALIGN_RIGHT);
    lv_obj_set_width(label, 100);
}

ysw_sdb_t *ysw_sdb_create(const char *title, void *context)
{
    ysw_sdb_t *sdb = ysw_heap_allocate(sizeof(ysw_sdb_t));
    sdb->win = lv_win_create(lv_scr_act(), NULL);
    sdb->context = context;
    lv_obj_set_user_data(sdb->win, sdb);

    lv_obj_align(sdb->win, NULL, LV_ALIGN_CENTER, 0, 0);
    //v7: lv_win_set_style(sdb->win, LV_WIN_STYLE_BG, &lv_style_pretty);
    //v7: lv_win_set_style(sdb->win, LV_WIN_STYLE_CONTENT, &ysw_style_sdb_content);
    lv_win_set_title(sdb->win, title);
    lv_win_set_layout(sdb->win, LV_LAYOUT_PRETTY_MID);

    lv_obj_t *close_btn = lv_win_add_btn(sdb->win, LV_SYMBOL_CLOSE);
    lv_obj_set_event_cb(close_btn, lv_win_close_event_cb);

    //v7: lv_win_set_btn_size(sdb->win, 20);
    lv_obj_set_event_cb(sdb->win, on_sdb_event);

    return sdb;
}

lv_obj_t *ysw_sdb_add_separator(ysw_sdb_t *sdb, const char *name)
{
    lv_obj_t *label = lv_label_create(sdb->win, NULL);
    lv_label_set_text(label, name);
    lv_label_set_align(label, LV_LABEL_ALIGN_CENTER);
    lv_obj_add_protect(label, LV_PROTECT_FOLLOW);
    return label;
}

lv_obj_t *ysw_sdb_add_string(ysw_sdb_t *sdb, const char *name, const char *value, void *cb)
{
    create_field_name(sdb, name);

    lv_obj_t *ta = lv_textarea_create(sdb->win, NULL);
    lv_obj_set_user_data(ta, cb);
    lv_obj_set_width(ta, 200);
    lv_obj_add_protect(ta, LV_PROTECT_FOLLOW);
    //v7: lv_textarea_set_style(ta, LV_TA_STYLE_BG, &ysw_style_white_cell);
    lv_textarea_set_one_line(ta, true);
    //v7.1: lv_textarea_set_cursor_type(ta, LV_CURSOR_LINE|LV_CURSOR_HIDDEN);
    lv_textarea_set_text(ta, value);
    lv_obj_set_event_cb(ta, on_ta_event);
    return ta;
}

lv_obj_t *ysw_sdb_add_choice(ysw_sdb_t *sdb, const char *name, uint8_t value, const char *options, void *cb)
{
    create_field_name(sdb, name);

    lv_obj_t *ddlist = lv_dropdown_create(sdb->win, NULL);
    lv_obj_set_user_data(ddlist, cb);
    //v7: lv_dropdown_set_anim_time(ddlist, 0);
    lv_dropdown_set_draw_arrow(ddlist, true);
    lv_dropdown_set_options(ddlist, options);
    //v7: lv_dropdown_ext_t *lv_dropdown_ext = lv_obj_get_ext_attr(ddlist);
    //v7: lv_coord_t height = lv_obj_get_height(lv_dropdown_ext->label);
    //v7: TODO: See if this is necessary with new max_height
    //v7: if (height > 100) {
    //v7:     lv_dropdown_set_max_height(ddlist, 100);
    //v7: }
    //v7: lv_dropdown_set_fix_width(ddlist, 200);
    lv_obj_set_width(ddlist, 200);
    //v7: lv_dropdown_set_style(ddlist, LV_DDLIST_STYLE_BG, &ysw_style_white_cell);
    //v7: lv_dropdown_set_align(ddlist, LV_LABEL_ALIGN_LEFT);
    lv_dropdown_set_selected(ddlist, value);
    lv_obj_add_protect(ddlist, LV_PROTECT_FOLLOW);
    lv_obj_set_event_cb(ddlist, on_ddlist_event);
    if (!ddlist_signal_cb) {
        ddlist_signal_cb = lv_obj_get_signal_cb(ddlist);
    }
    lv_obj_set_signal_cb(ddlist, on_ddlist_signal);
    return ddlist;
}

lv_obj_t *ysw_sdb_add_switch(ysw_sdb_t *sdb, const char *name, bool value, void *cb)
{
    create_field_name(sdb, name);

    lv_obj_t *sw = lv_switch_create(sdb->win, NULL);
    lv_obj_set_user_data(sw, cb);
    lv_obj_set_width(sw, 80);
    lv_obj_add_protect(sw, LV_PROTECT_FOLLOW);
    if (value) {
        lv_switch_on(sw, LV_ANIM_OFF);
    } else {
        lv_switch_off(sw, LV_ANIM_OFF);
    }
    lv_obj_set_event_cb(sw, on_sw_event);
    return sw;
}

lv_obj_t *ysw_sdb_add_checkbox(ysw_sdb_t *sdb, const char *name, bool value, void *callback)
{
    lv_obj_t *cb = lv_checkbox_create(sdb->win, NULL);
    lv_checkbox_set_text(cb, name);
    lv_obj_set_user_data(cb, callback);
    lv_obj_add_protect(cb, LV_PROTECT_FOLLOW);
    lv_checkbox_set_checked(cb, value);
    lv_obj_set_event_cb(cb, on_cb_event);
    return cb;
}

lv_obj_t *ysw_sdb_add_button(ysw_sdb_t *sdb, const char *name, void *callback)
{
    lv_obj_t *btn = lv_btn_create(sdb->win, NULL);
    //v7: lv_btn_set_style(btn, LV_BTN_STYLE_REL, &ysw_style_btn_rel);
    //v7: lv_btn_set_style(btn, LV_BTN_STYLE_PR, &ysw_style_btn_pr);
    lv_obj_set_width(btn, 140);
    lv_btn_set_fit2(btn, LV_FIT_NONE, LV_FIT_TIGHT);
    lv_obj_set_user_data(btn, callback);
    lv_obj_set_event_cb(btn, on_btn_event);

    lv_obj_t *label = lv_label_create(btn, NULL);
    lv_label_set_text(label, name);
    return btn;
}

void ysw_sdb_close(ysw_sdb_t *sdb)
{
    lv_obj_del(sdb->win); // on_sdb_event will free sdb
}

