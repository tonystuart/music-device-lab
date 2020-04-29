// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "lvgl/lvgl.h"
#include "ysw_lv_cne.h"

typedef struct {
    lv_event_cb_t next_cb;
    lv_event_cb_t prev_cb;
    ysw_lv_cse_event_cb_t cse_event_cb;
} ysw_csef_config_t;

typedef struct {
    lv_obj_t *cse;
    lv_obj_t *win;
    lv_obj_t *footer_label;
} ysw_csef_t;

ysw_csef_t *ysw_csef_create(ysw_csef_config_t *config);
void ysw_csef_free(ysw_csef_t *csef);
void ysw_csef_set_cs(ysw_csef_t *csef, ysw_cs_t *cs);
void ysw_csef_set_header_text(ysw_csef_t *csef, char *header_text);
void ysw_csef_set_footer_text(ysw_csef_t *csef, char *footer_text);
