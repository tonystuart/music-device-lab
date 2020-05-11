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

#define YSW_FRAME_H

typedef struct {
    lv_obj_t *win;
    lv_obj_t *footer;
    lv_obj_t *footer_label;
    lv_obj_t *last_button;
} ysw_frame_t;

typedef lv_obj_t *(*ysw_frame_create_cb_t)(lv_obj_t *parent);

void ysw_frame_set_footer_text(ysw_frame_t *frame, const char *footer_text);
void ysw_frame_set_header_text(ysw_frame_t *frame, const char *header_text);
void ysw_frame_free(ysw_frame_t *frame);
lv_obj_t *ysw_frame_create_content(ysw_frame_t *frame, ysw_frame_create_cb_t create_cb);
lv_obj_t *ysw_frame_add_footer_button(ysw_frame_t *frame, const void *img_src, lv_event_cb_t event_cb);
lv_obj_t *ysw_frame_add_header_button(ysw_frame_t *frame, const void *img_src, lv_event_cb_t event_cb);
ysw_frame_t *ysw_frame_create();

