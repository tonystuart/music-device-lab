// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "esp_log.h"
#include "ysw_style.h"

#define TAG "YSW_STYLE"

lv_draw_line_dsc_t line_dsc;
lv_draw_rect_dsc_t rect_dsc;
lv_draw_label_dsc_t label_dsc;

lv_draw_line_dsc_t odd_line_dsc;
lv_draw_line_dsc_t even_line_dsc;
lv_draw_line_dsc_t div_line_dsc;
lv_draw_line_dsc_t metro_line_dsc;

lv_draw_rect_dsc_t odd_rect_dsc;
lv_draw_rect_dsc_t even_rect_dsc;
lv_draw_rect_dsc_t sn_rect_dsc;
lv_draw_rect_dsc_t sel_sn_rect_dsc;
lv_draw_rect_dsc_t drag_sn_rect_dsc;

lv_draw_label_dsc_t sn_label_dsc;
lv_draw_label_dsc_t sel_sn_label_dsc;
lv_draw_label_dsc_t drag_sn_label_dsc;

void ysw_style_initialize()
{
    lv_draw_line_dsc_init(&line_dsc);
    //line_dsc.color = LV_COLOR_CYAN;
    //lv_obj_init_draw_line_dsc(table, 0, &line_dsc);

    lv_draw_rect_dsc_init(&rect_dsc);
    //rect_dsc.border_color = LV_COLOR_RED;
    //lv_obj_init_draw_rect_dsc(table, 0, &rect_dsc);

    lv_draw_label_dsc_init(&label_dsc);
    //label_dsc.color = LV_COLOR_YELLOW;
    //lv_obj_init_draw_label_dsc(table, 0, &label_dsc);

    odd_line_dsc = line_dsc;
    even_line_dsc = line_dsc;

    div_line_dsc = (lv_draw_line_dsc_t ) {
                .color = LV_COLOR_BLACK,
                .width = 1,
                .dash_width = 2,
                .dash_gap = 2,
                .opa = LV_OPA_COVER,
            };

    metro_line_dsc = line_dsc;

    odd_rect_dsc = (lv_draw_rect_dsc_t ) {
                .bg_color = LV_COLOR_MAKE(0x80, 0x8d, 0x94),
                .bg_opa = LV_OPA_COVER,
            };

    even_rect_dsc = (lv_draw_rect_dsc_t ) {
                .bg_color = LV_COLOR_MAKE(0xe1, 0xef, 0xf7),
                .bg_opa = LV_OPA_COVER,
            };

    sn_rect_dsc = (lv_draw_rect_dsc_t ) {
                .radius = 4,
                .bg_color = LV_COLOR_PURPLE,
                .bg_opa = LV_OPA_50,
                .border_color = LV_COLOR_PURPLE,
                .border_opa = LV_OPA_COVER,
                .border_width = 1,
                .border_side = LV_BORDER_SIDE_FULL,
            };

    sel_sn_rect_dsc = (lv_draw_rect_dsc_t ) {
                .radius = 4,
                .bg_color = LV_COLOR_LIME,
                .bg_opa = LV_OPA_70,
                .border_color = LV_COLOR_GREEN,
                .border_opa = LV_OPA_COVER,
                .border_width = 1,
                .border_side = LV_BORDER_SIDE_FULL,
            };

    drag_sn_rect_dsc = (lv_draw_rect_dsc_t ) {
                .radius = 4,
                .bg_color = LV_COLOR_LIME,
                .bg_opa = LV_OPA_50,
                .border_color = LV_COLOR_LIME,
                .border_opa = LV_OPA_COVER,
                .border_width = 1,
                .border_side = LV_BORDER_SIDE_FULL,
            };

    sn_label_dsc = (lv_draw_label_dsc_t ) {
                .color = LV_COLOR_WHITE,
                .opa = LV_OPA_COVER,
                .font = &lv_font_montserrat_12,
                .flag = LV_TXT_FLAG_CENTER,

            };

    sel_sn_label_dsc = (lv_draw_label_dsc_t ) {
                .color = LV_COLOR_BLACK,
                .opa = LV_OPA_COVER,
                .font = &lv_font_montserrat_12,
                .flag = LV_TXT_FLAG_CENTER,

            };

    drag_sn_label_dsc = sel_sn_label_dsc;
}

void ysw_style_lighten_background(lv_obj_t *obj)
{
    lv_color_t parent_bg = LV_COLOR_BLACK;
    lv_obj_t *parent = lv_obj_get_parent(obj);
    if (parent) {
        parent_bg = lv_obj_get_style_bg_color(parent, LV_OBJ_PART_MAIN);
    }
    lv_color_t child_bg = lv_color_lighten(parent_bg, LV_OPA_20);
    lv_obj_set_style_local_bg_color(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, child_bg);
}

void ysw_style_clear_border(lv_obj_t *obj)
{
    lv_obj_set_style_local_border_width(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
}

void ysw_style_adjust_obj(lv_obj_t *obj)
{
    ysw_style_lighten_background(obj);
    ysw_style_clear_border(obj);
}

void ysw_style_adjust_field_name(lv_obj_t *label)
{
    lv_obj_set_style_local_pad_top(label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 10);
    lv_obj_set_style_local_pad_bottom(label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 2);
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
    lv_obj_set_style_local_pad_left(kb, LV_KEYBOARD_PART_BG, LV_STATE_DEFAULT, 10);
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
    ysw_style_lighten_background(btn);
    lv_obj_set_style_local_margin_top(btn, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 5);
    lv_obj_set_style_local_margin_bottom(btn, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 5);
}

