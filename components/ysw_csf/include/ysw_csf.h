// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "lvgl/lvgl.h"
#include "ysw_lv_cse.h"
#include "ysw_frame.h"

typedef struct {
    lv_event_cb_t next_cb;
    lv_event_cb_t play_cb;
    lv_event_cb_t pause_cb;
    lv_event_cb_t loop_cb;
    lv_event_cb_t prev_cb;
    lv_event_cb_t close_cb;
    lv_event_cb_t settings_cb;
    lv_event_cb_t save_cb;
    lv_event_cb_t new_cb;
    lv_event_cb_t copy_cb;
    lv_event_cb_t paste_cb;
    lv_event_cb_t volume_mid_cb;
    lv_event_cb_t volume_max_cb;
    lv_event_cb_t trash_cb;
    ysw_lv_cse_event_cb_t cse_event_cb;
} ysw_csf_config_t;

typedef struct {
    ysw_frame_t *frame;
    lv_obj_t *cse;
} ysw_csf_t;

ysw_csf_t *ysw_csf_create(ysw_csf_config_t *config);
void ysw_csf_free(ysw_csf_t *csf);
void ysw_csf_set_cs(ysw_csf_t *csf, ysw_cs_t *cs);
void ysw_csf_redraw(ysw_csf_t *csf);
void ysw_csf_set_header_text(ysw_csf_t *csf, const char *header_text);
void ysw_csf_set_footer_text(ysw_csf_t *csf, const char *footer_text);
