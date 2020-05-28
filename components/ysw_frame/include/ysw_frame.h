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
    lv_obj_t *container;
    lv_obj_t *win;
    lv_obj_t *footer;
    lv_obj_t *footer_label;
    lv_obj_t *last_button;
} ysw_frame_t;

typedef void (*ysw_frame_cb_t)(void *context);

ysw_frame_t *ysw_frame_create(void *context);
lv_obj_t *ysw_frame_add_header_button(ysw_frame_t *frame, const void *img_src, void *cb);
lv_obj_t *ysw_frame_add_footer_button(ysw_frame_t *frame, const void *img_src, void *cb);
void ysw_frame_set_header_text(ysw_frame_t *frame, const char *header_text);
void ysw_frame_set_footer_text(ysw_frame_t *frame, const char *footer_text);
void ysw_frame_set_content(ysw_frame_t *frame, lv_obj_t *object);
void ysw_frame_del(ysw_frame_t *frame);

