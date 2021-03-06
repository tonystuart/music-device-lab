// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_style.h"
#include "esp_log.h"

#define TAG "YSW_STYLE"

// See https://docs.lvgl.io/v7/en/html/overview/style.html

// See https://material.io/design/color/the-color-system.html
// See https://material.io/resources/color/

void ysw_style_clear_border(lv_obj_t *obj)
{
    lv_obj_set_style_local_border_width(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
}

lv_color_t ysw_style_get_child_background(lv_obj_t *obj)
{
    lv_color_t parent_bg = LV_COLOR_BLACK;
    lv_obj_t *parent = lv_obj_get_parent(obj);
    if (parent) {
        parent_bg = lv_obj_get_style_bg_color(parent, LV_OBJ_PART_MAIN);
    }
    lv_color_t child_bg = lv_color_lighten(parent_bg, LV_OPA_20);
    return child_bg;
}

lv_color_t ysw_style_lighten_background(lv_obj_t *obj)
{
    lv_color_t child_bg = ysw_style_get_child_background(obj);
    lv_obj_set_style_local_bg_color(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, child_bg);
    return child_bg;
}

lv_color_t ysw_style_adjust_obj(lv_obj_t *obj)
{
    lv_color_t child_bg = ysw_style_lighten_background(obj);
    ysw_style_clear_border(obj);
    return child_bg;
}

void ysw_style_adjust_field_name(lv_obj_t *label)
{
    lv_obj_set_style_local_pad_top(label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 5);
    lv_obj_set_style_local_pad_bottom(label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_text_font(label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, &lv_font_montserrat_14);
}

void ysw_style_adjust_keyboard(lv_obj_t *kb)
{
    lv_color_t parent_bg = LV_COLOR_BLACK;
    lv_obj_t *parent = lv_obj_get_parent(kb);
    if (parent) {
        parent_bg = lv_obj_get_style_bg_color(parent, LV_OBJ_PART_MAIN);
    }
    lv_color_t child_bg = lv_color_lighten(parent_bg, LV_OPA_20);
    lv_color_t btn_bg = lv_color_lighten(child_bg, LV_OPA_20);
    lv_obj_set_style_local_bg_color(kb, LV_KEYBOARD_PART_BG, LV_STATE_DEFAULT, child_bg);
    lv_obj_set_style_local_bg_color(kb, LV_KEYBOARD_PART_BTN, LV_STATE_DEFAULT, btn_bg);
    lv_obj_set_style_local_border_width(kb, LV_KEYBOARD_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_border_width(kb, LV_KEYBOARD_PART_BTN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_left(kb, LV_KEYBOARD_PART_BG, LV_STATE_DEFAULT, 5);
}

void ysw_style_adjust_ddlist(lv_obj_t *ddlist)
{
    lv_color_t parent_bg = LV_COLOR_BLACK;
    lv_obj_t *parent = lv_obj_get_parent(ddlist);
    if (parent) {
        parent_bg = lv_obj_get_style_bg_color(parent, LV_OBJ_PART_MAIN);
    }
    lv_color_t child_bg = lv_color_lighten(parent_bg, LV_OPA_20);
    lv_color_t btn_bg = lv_color_lighten(child_bg, LV_OPA_20);
    lv_obj_set_style_local_bg_color(ddlist, LV_PAGE_PART_BG, LV_STATE_DEFAULT, child_bg);
    lv_obj_set_style_local_bg_color(ddlist, LV_PAGE_PART_SCROLLABLE, LV_STATE_DEFAULT, btn_bg);
    lv_obj_set_style_local_border_width(ddlist, LV_PAGE_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_border_color(ddlist, LV_PAGE_PART_SCROLLABLE, LV_STATE_DEFAULT, LV_COLOR_BLACK);
}

void ysw_style_adjust_btn(lv_obj_t *btn)
{
    ysw_style_adjust_obj(btn);
    lv_obj_set_style_local_margin_top(btn, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 5);
    lv_obj_set_style_local_margin_bottom(btn, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 5);
}

void ysw_style_adjust_button_bar(lv_obj_t *bar)
{
    lv_color_t child_bg = ysw_style_adjust_obj(bar);
    lv_color_t btn_bg = lv_color_lighten(child_bg, LV_OPA_20);
    lv_obj_set_style_local_bg_color(bar, LV_BTNMATRIX_PART_BTN, LV_STATE_DEFAULT, btn_bg);
    //lv_obj_set_style_local_text_font(bar, LV_BTNMATRIX_PART_BTN, LV_STATE_DEFAULT, &lv_font_montserrat_14);
}

void ysw_style_adjust_checkbox(lv_obj_t *cb)
{
    lv_color_t child_bg = ysw_style_lighten_background(cb);
    lv_obj_set_style_local_text_font(cb, LV_CHECKBOX_PART_BG, LV_STATE_DEFAULT, &lv_font_montserrat_14);
    lv_obj_set_style_local_pad_left(cb, LV_CHECKBOX_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_border_width(cb, LV_CHECKBOX_PART_BULLET, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_bg_color(cb, LV_CHECKBOX_PART_BULLET, LV_STATE_DEFAULT, child_bg);
    lv_obj_set_style_local_bg_color(cb, LV_CHECKBOX_PART_BULLET, LV_STATE_CHECKED, child_bg);
    lv_obj_set_style_local_pattern_image(cb, LV_CHECKBOX_PART_BULLET, LV_STATE_CHECKED, LV_SYMBOL_OK);
    lv_obj_set_style_local_pattern_recolor(cb, LV_CHECKBOX_PART_BULLET, LV_STATE_CHECKED, LV_COLOR_WHITE);
    lv_obj_set_style_local_pad_top(cb, LV_CHECKBOX_PART_BULLET, LV_STATE_DEFAULT, 2);
    lv_obj_set_style_local_pad_right(cb, LV_CHECKBOX_PART_BULLET, LV_STATE_DEFAULT, 2);
    lv_obj_set_style_local_pad_bottom(cb, LV_CHECKBOX_PART_BULLET, LV_STATE_DEFAULT, 2);
    lv_obj_set_style_local_pad_left(cb, LV_CHECKBOX_PART_BULLET, LV_STATE_DEFAULT, 2);
}

void ysw_style_adjust_mbox(lv_obj_t *mbox)
{
    lv_obj_set_style_local_bg_color(mbox, LV_MSGBOX_PART_BG, LV_STATE_DEFAULT, YSW_STYLE_COLOR(0xc0c0c0));
    lv_obj_set_style_local_text_color(mbox, LV_MSGBOX_PART_BG, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_obj_set_style_local_border_width(mbox, LV_MSGBOX_PART_BG, LV_STATE_DEFAULT, 0);

    lv_obj_set_style_local_shadow_width(mbox, LV_MSGBOX_PART_BG, LV_STATE_DEFAULT, 50);
    lv_obj_set_style_local_shadow_spread(mbox, LV_MSGBOX_PART_BG, LV_STATE_DEFAULT, 10);
    lv_obj_set_style_local_shadow_color(mbox, LV_MSGBOX_PART_BG, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    lv_obj_set_style_local_shadow_opa(mbox, LV_MSGBOX_PART_BG, LV_STATE_DEFAULT, LV_OPA_COVER);

    lv_obj_set_style_local_bg_color(mbox, LV_MSGBOX_PART_BTN, LV_STATE_DEFAULT, YSW_STYLE_COLOR(0xf5f5f5));
    lv_obj_set_style_local_text_color(mbox, LV_MSGBOX_PART_BTN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_obj_set_style_local_border_width(mbox, LV_MSGBOX_PART_BTN, LV_STATE_DEFAULT, 0);
}

static void style_container(lv_obj_t *container)
{
    lv_obj_set_style_local_bg_color(container, 0, 0, LV_COLOR_MAROON);
    lv_obj_set_style_local_bg_grad_color(container, 0, 0, LV_COLOR_BLACK);
    lv_obj_set_style_local_bg_grad_dir(container, 0, 0, LV_GRAD_DIR_VER);
    lv_obj_set_style_local_bg_opa(container, 0, 0, LV_OPA_100);
    lv_obj_set_style_local_text_color(container, 0, 0, LV_COLOR_YELLOW);
    lv_obj_set_style_local_text_opa(container, 0, 0, LV_OPA_100);
}

void ysw_style_editor(lv_obj_t *container)
{
    style_container(container);
}

void ysw_style_chooser(lv_obj_t *page, lv_obj_t *table)
{
    style_container(page);

    lv_obj_set_style_local_pad_top(table, NORMAL_CELL, 0, 5);

    lv_obj_set_style_local_margin_bottom(table, HEADING_CELL, 0, 20);
    lv_obj_set_style_local_pad_top(table, HEADING_CELL, 0, 5);
    lv_obj_set_style_local_pad_bottom(table, HEADING_CELL, 0, 5);

    lv_obj_set_style_local_bg_color(table, SELECTED_CELL, 0, LV_COLOR_GRAY);
    lv_obj_set_style_local_bg_opa(table, SELECTED_CELL, 0, LV_OPA_80);
}

void ysw_style_softkeys(lv_obj_t *container, lv_obj_t *label, lv_obj_t *btnmatrix)
{
    style_container(container);

    lv_obj_set_style_local_text_font(label, 0, 0, &lv_font_unscii_8);

    lv_color_t child_bg = lv_color_lighten(LV_COLOR_MAROON, LV_OPA_40);
    lv_obj_set_style_local_bg_color(btnmatrix, LV_BTNMATRIX_PART_BTN, 0, child_bg);
    lv_obj_set_style_local_bg_opa(btnmatrix, LV_BTNMATRIX_PART_BTN, 0, LV_OPA_50);

    lv_obj_set_style_local_bg_color(btnmatrix, LV_BTNMATRIX_PART_BTN, LV_STATE_PRESSED, LV_COLOR_BLACK);
    lv_obj_set_style_local_bg_opa(btnmatrix, LV_BTNMATRIX_PART_BTN, LV_STATE_PRESSED, LV_OPA_50);

    lv_obj_set_style_local_pad_all(btnmatrix, LV_BTNMATRIX_PART_BG, 0, 5);
    lv_obj_set_style_local_pad_inner(btnmatrix, LV_BTNMATRIX_PART_BG, 0, 5);
}

void ysw_style_softkeys_lucid(lv_obj_t *container, lv_obj_t *label, lv_obj_t *btnmatrix)
{
    ysw_style_softkeys(container, label, btnmatrix);

    lv_obj_set_style_local_bg_opa(container, 0, 0, LV_OPA_30);
}

void ysw_style_softkeys_hidden(lv_obj_t *container, lv_obj_t *label, lv_obj_t *btnmatrix)
{
    lv_obj_set_style_local_bg_opa(container, 0, 0, LV_OPA_0);
    lv_obj_set_style_local_text_opa(container, 0, 0, LV_OPA_0);
    lv_obj_set_style_local_bg_opa(btnmatrix, LV_BTNMATRIX_PART_BTN, 0, LV_OPA_0);
    lv_obj_set_style_local_bg_opa(btnmatrix, LV_BTNMATRIX_PART_BTN, LV_STATE_PRESSED, LV_OPA_0);
}

void ysw_style_popup(lv_obj_t *container, lv_obj_t *msgbox)
{
    style_container(container);

    lv_obj_set_style_local_pad_all(container, 0, 0, 5);
    lv_obj_set_style_local_pad_inner(container, 0, 0, 5);

    lv_obj_set_style_local_pad_all(msgbox, 0, 0, 5);
    lv_obj_set_style_local_pad_inner(msgbox, 0, 0, 10);

    lv_color_t child_bg = lv_color_lighten(LV_COLOR_MAROON, LV_OPA_40);
    lv_obj_set_style_local_bg_color(msgbox, LV_MSGBOX_PART_BTN, 0, child_bg);
    lv_obj_set_style_local_bg_opa(msgbox, LV_MSGBOX_PART_BTN, 0, LV_OPA_50);
    lv_obj_set_style_local_pad_all(msgbox, LV_MSGBOX_PART_BTN, 0, 10);

#if 0
    // This doesn't leave any padding between the buttons in the message box:
    lv_obj_set_style_local_pad_inner(msgbox, LV_MSGBOX_PART_BTN_BG, 0, 10);
#else
    // But this does:
    lv_msgbox_ext_t *ext = lv_obj_get_ext_attr(msgbox);
    lv_obj_set_style_local_pad_inner(ext->btnm, LV_BTNMATRIX_PART_BG, 0, 10);
#endif

    lv_obj_set_style_local_bg_color(msgbox, LV_MSGBOX_PART_BTN, LV_STATE_PRESSED, LV_COLOR_BLACK);
    lv_obj_set_style_local_bg_opa(msgbox, LV_MSGBOX_PART_BTN, LV_STATE_PRESSED, LV_OPA_50);
}

void ysw_style_field(lv_obj_t *name_label, lv_obj_t *field)
{
    lv_obj_set_style_local_text_font(name_label, 0, 0, &lv_font_unscii_8);
    lv_obj_set_style_local_pad_top(field, 0, 0, 2);
    lv_obj_set_style_local_pad_left(field, 0, 0, 2);
}

void ysw_style_edit_pane(lv_obj_t *container, lv_obj_t *textarea, lv_obj_t *keyboard)
{
    style_container(container);

    lv_obj_set_style_local_pad_all(container, 0, 0, 5);
    lv_obj_set_style_local_pad_inner(container, 0, 0, 5);

    lv_color_t child_bg = lv_color_lighten(LV_COLOR_MAROON, LV_OPA_40);

    lv_obj_set_style_local_text_color(textarea, 0, 0, LV_COLOR_WHITE);
    lv_obj_set_style_local_bg_color(textarea, 0, 0, child_bg);
    lv_obj_set_style_local_bg_opa(textarea, 0, 0, LV_OPA_50);
    lv_obj_set_style_local_pad_all(textarea, 0, 0, 5);

    lv_obj_set_style_local_border_width(textarea, LV_TEXTAREA_PART_CURSOR, 0, 1);
    lv_obj_set_style_local_border_side(textarea, LV_TEXTAREA_PART_CURSOR, 0, LV_BORDER_SIDE_LEFT);
    lv_obj_set_style_local_border_color(textarea, LV_TEXTAREA_PART_CURSOR, 0, LV_COLOR_WHITE);
    lv_obj_set_style_local_border_opa(textarea, LV_TEXTAREA_PART_CURSOR, 0, LV_OPA_100);

    lv_obj_set_style_local_bg_color(keyboard, LV_KEYBOARD_PART_BTN, 0, child_bg);
    lv_obj_set_style_local_bg_opa(keyboard, LV_KEYBOARD_PART_BTN, 0, LV_OPA_50);
    lv_obj_set_style_local_border_width(keyboard, LV_KEYBOARD_PART_BG, 0, 0);
    lv_obj_set_style_local_border_width(keyboard, LV_KEYBOARD_PART_BTN, 0, 0);
    lv_obj_set_style_local_pad_all(keyboard, LV_KEYBOARD_PART_BG, 0, 5);
    lv_obj_set_style_local_pad_inner(keyboard, LV_KEYBOARD_PART_BG, 0, 5);
}

void ysw_style_splash(lv_obj_t *container, lv_obj_t *label, lv_obj_t *image)
{
    style_container(container);
}

