// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "lvgl.h"
#include "stdint.h"

#define YSW_UI_BUTTONS 10

typedef void (*ysw_ui_btn_cb_t)(void *context, lv_obj_t *btn);

typedef struct {
    ysw_ui_btn_cb_t cb;
    void *context;
} ysw_ui_cbd_t;

typedef struct {
    void *img_src;
    void *btn_cb;
} ysw_ui_btn_def_t;

typedef struct {
    void *img_src;
    uint32_t msg;
} ysw_ui_bus_btn_def_t;

typedef struct {
    lv_obj_t *label;
} ysw_ui_label_t;

typedef struct {
    void *img_src;
    lv_obj_t *btn;
    ysw_ui_cbd_t cbd;
} ysw_ui_button_t;

typedef struct {
    lv_obj_t *container;
    ysw_ui_label_t title;
    ysw_ui_button_t buttons[YSW_UI_BUTTONS];
} ysw_ui_header_t;

typedef struct {
    lv_obj_t *container;
    ysw_ui_label_t info;
    ysw_ui_button_t buttons[YSW_UI_BUTTONS];
} ysw_ui_footer_t;

typedef struct {
    lv_obj_t *page;
} ysw_ui_body_t;

typedef struct {
    lv_obj_t *container;
    ysw_ui_header_t header;
    ysw_ui_body_t body;
    ysw_ui_footer_t footer;
} ysw_ui_frame_t;

lv_coord_t ysw_ui_get_extra_width(lv_obj_t *parent);
void ysw_ui_distribute_extra_width(lv_obj_t *parent, lv_obj_t *obj);
lv_coord_t ysw_ui_get_extra_height(lv_obj_t *parent);
void ysw_ui_distribute_extra_height(lv_obj_t *parent, lv_obj_t *obj);
void ysw_ui_get_obj_type(lv_obj_t *obj, char *buffer, uint32_t size);
int32_t ysw_ui_get_index_of_child(lv_obj_t *obj);
lv_obj_t* ysw_ui_child_at_index(lv_obj_t *parent, uint32_t index);
void ysw_ui_ensure_visible(lv_obj_t *child, bool do_center);
void ysw_ui_init_cbd(ysw_ui_cbd_t *cbd, void *cb, void *context);
void ysw_ui_init_button(ysw_ui_button_t *button, void *img_src, ysw_ui_btn_cb_t cb, void *context);
void ysw_ui_init_buttons(ysw_ui_button_t buttons[], const ysw_ui_btn_def_t defs[], void *context);
void ysw_ui_init_bus_buttons(ysw_ui_button_t buttons[], const ysw_ui_bus_btn_def_t defs[]);
void ysw_ui_create_label(lv_obj_t *parent, ysw_ui_label_t *label);
void ysw_ui_on_btn_event(lv_obj_t *btn, lv_event_t event);
void ysw_ui_create_button(lv_obj_t *parent, ysw_ui_button_t *button);
void ysw_ui_create_header(lv_obj_t *parent, ysw_ui_header_t *header);
void ysw_ui_set_header_text(ysw_ui_header_t *header, const char *text);
void ysw_ui_create_body(lv_obj_t *parent, ysw_ui_body_t *body);
void ysw_ui_create_footer(lv_obj_t *parent, ysw_ui_footer_t *footer);
void ysw_ui_set_footer_text(ysw_ui_footer_t *footer, const char *text);
void ysw_ui_create_frame(ysw_ui_frame_t *frame);
void ysw_ui_close_frame(ysw_ui_frame_t *frame);

