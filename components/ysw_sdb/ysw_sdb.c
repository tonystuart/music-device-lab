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
#include "ysw_style.h"

#include "stdio.h"
#include "esp_log.h"
#include "lvgl.h"

#define TAG "YSW_SDB"

// Key to SDB context management:
// We stash the context in the sdb
// We stash the sdb in the body's page's scrollable user data
// We stash the callback in each widget's user data
// We fetch the context via the sdb using the parent relationship

static ysw_sdb_t *get_sdb(lv_obj_t *field)
{
    lv_obj_t *scrl = lv_obj_get_parent(field);
    ysw_sdb_t *sdb = lv_obj_get_user_data(scrl);
    return sdb;
}

static void on_kb_event(lv_obj_t *keyboard, lv_event_t event)
{
    lv_keyboard_def_event_cb(keyboard, event);

    if (event == LV_EVENT_APPLY || event == LV_EVENT_CANCEL) {
        lv_obj_t *ta = lv_keyboard_get_textarea(keyboard);
        if (ta) {
            //v7.1: lv_textarea_set_cursor_type(ta, LV_CURSOR_LINE|LV_CURSOR_HIDDEN);
            ysw_sdb_t *sdb = get_sdb(ta);
            lv_obj_del(sdb->controller.kb);
            sdb->controller.kb = NULL;
        }
    }
}

static void on_ta_event(lv_obj_t *ta, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        ysw_sdb_t *sdb = get_sdb(ta);
        lv_obj_t *container = lv_page_get_scrl(sdb->frame.body.page);
        lv_cont_set_layout(container, LV_LAYOUT_OFF);
        if (!sdb->controller.kb) {
            sdb->controller.kb = lv_keyboard_create(sdb->frame.body.page, NULL);
            lv_obj_set_size(sdb->controller.kb, 280, 100);
            ysw_style_adjust_keyboard(sdb->controller.kb);
            lv_obj_set_event_cb(sdb->controller.kb, on_kb_event);
            lv_keyboard_set_cursor_manage(sdb->controller.kb, true);
        }
        if (lv_keyboard_get_textarea(sdb->controller.kb) == ta) {
            // click again in same field - hide the kb
            //v7.1: lv_textarea_set_cursor_type(ta, LV_CURSOR_LINE|LV_CURSOR_HIDDEN);
            lv_obj_del(sdb->controller.kb);
            sdb->controller.kb = NULL;
        } else {
            lv_keyboard_set_textarea(sdb->controller.kb, ta);
            _lv_ll_move_before(&container->child_ll, sdb->controller.kb, ta);
        }
        lv_cont_set_layout(container, LV_LAYOUT_COLUMN_LEFT);
    }
    if (event == LV_EVENT_VALUE_CHANGED) {
        const char *new_value = lv_textarea_get_text(ta);
        ysw_sdb_string_cb_t cb = lv_obj_get_user_data(ta);
        if (cb) {
            cb(get_sdb(ta)->controller.context, new_value);
        }
    }
}

static void on_ddlist_event(lv_obj_t *ddlist, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        uint8_t new_value = lv_dropdown_get_selected(ddlist);
        ysw_sdb_choice_cb_t cb = lv_obj_get_user_data(ddlist);
        if (cb) {
            cb(get_sdb(ddlist)->controller.context, new_value);
        }
    }
}

static void on_sw_event(lv_obj_t *sw, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        bool new_value = lv_switch_get_state(sw);
        ysw_sdb_switch_cb_t cb = lv_obj_get_user_data(sw);
        if (cb) {
            cb(get_sdb(sw)->controller.context, new_value);
        }
    }
}

static void on_cb_event(lv_obj_t *cb, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        bool new_value = lv_checkbox_is_checked(cb);
        ysw_sdb_checkbox_cb_t callback = lv_obj_get_user_data(cb);
        if (callback) {
            callback(get_sdb(cb)->controller.context, new_value);
        }
    }
}

static void on_btn_event(lv_obj_t *btn, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        ysw_sdb_button_cb_t cb = lv_obj_get_user_data(btn);
        if (cb) {
            cb(get_sdb(btn)->controller.context);
        }
    }
}

static void on_close(ysw_sdb_t *sdb, lv_obj_t *btn)
{
    lv_obj_del(sdb->frame.container); // deletes contents
    ysw_heap_free(sdb);
}

static void create_field_name(ysw_sdb_t *sdb, const char *name)
{
    lv_obj_t *label = lv_label_create(sdb->frame.body.page, NULL);
    lv_label_set_text(label, name);
    lv_obj_set_style_local_pad_top(label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 10);
    lv_obj_set_style_local_pad_bottom(label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 2);
}

static const ysw_ui_btn_def_t header_buttons[] = {
        { LV_SYMBOL_CLOSE, on_close },
        { NULL, NULL },
};

ysw_sdb_t *ysw_sdb_create(lv_obj_t *parent, const char *title, void *context)
{
    ysw_sdb_t *sdb = ysw_heap_allocate(sizeof(ysw_sdb_t)); // freed in on_close
    sdb->controller.context = context;
    ysw_ui_init_buttons(sdb->frame.header.buttons, header_buttons, sdb);
    ysw_ui_create_frame(&sdb->frame, parent);
    ysw_ui_set_header_text(&sdb->frame.header, title);
    lv_obj_set_user_data(lv_page_get_scrl(sdb->frame.body.page), sdb);
    lv_page_set_scrl_layout(sdb->frame.body.page, LV_LAYOUT_COLUMN_LEFT);
    return sdb;
}

lv_obj_t *ysw_sdb_add_separator(ysw_sdb_t *sdb, const char *name)
{
    lv_obj_t *label = lv_label_create(sdb->frame.body.page, NULL);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CROP);
    lv_obj_set_size(label, lv_page_get_width_fit(sdb->frame.body.page), 30);
    lv_obj_set_style_local_pad_top(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 10);
    lv_label_set_align(label, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text(label, name);
    return label;
}

lv_obj_t *ysw_sdb_add_string(ysw_sdb_t *sdb, const char *name, const char *value, void *cb)
{
    create_field_name(sdb, name);

    lv_obj_t *ta = lv_textarea_create(sdb->frame.body.page, NULL);
    lv_obj_set_width(ta, lv_page_get_width_fit(sdb->frame.body.page));
    ysw_style_adjust_obj(ta);
    lv_obj_set_user_data(ta, cb);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_text(ta, value);
    lv_obj_set_event_cb(ta, on_ta_event);
    return ta;
}

lv_obj_t *ysw_sdb_add_choice(ysw_sdb_t *sdb, const char *name, uint8_t value, const char *options, void *cb)
{
    create_field_name(sdb, name);

    lv_obj_t *ddlist = lv_dropdown_create(sdb->frame.body.page, NULL);
    ysw_style_adjust_ddlist(ddlist);
    lv_obj_set_user_data(ddlist, cb);
    lv_dropdown_set_options(ddlist, options);
    lv_coord_t w = lv_page_get_width_fit(sdb->frame.body.page);
    lv_obj_set_width(ddlist, w);
    lv_dropdown_set_max_height(ddlist, 100);
    lv_dropdown_set_selected(ddlist, value);
    lv_dropdown_set_draw_arrow(ddlist, true); // forces left align
    lv_obj_set_event_cb(ddlist, on_ddlist_event);
    return ddlist;
}

lv_obj_t *ysw_sdb_add_switch(ysw_sdb_t *sdb, const char *name, bool value, void *cb)
{
    create_field_name(sdb, name);

    lv_obj_t *sw = lv_switch_create(sdb->frame.body.page, NULL);
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
    lv_obj_t *cb = lv_checkbox_create(sdb->frame.body.page, NULL);
    lv_checkbox_set_text(cb, name);
    lv_obj_set_user_data(cb, callback);
    lv_obj_add_protect(cb, LV_PROTECT_FOLLOW);
    lv_checkbox_set_checked(cb, value);
    lv_obj_set_event_cb(cb, on_cb_event);
    return cb;
}

lv_obj_t *ysw_sdb_add_button(ysw_sdb_t *sdb, const char *name, void *callback)
{
    lv_obj_t *btn = lv_btn_create(sdb->frame.body.page, NULL);
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


