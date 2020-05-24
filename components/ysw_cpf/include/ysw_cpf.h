// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "lvgl/lvgl.h"
#include "ysw_lv_cpe.h"
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
    lv_event_cb_t trash_cb;
    lv_event_cb_t left_cb;
    lv_event_cb_t right_cb;
    ysw_lv_cpe_event_cb_t cpe_event_cb;
} ysw_cpf_config_t;

typedef struct {
    ysw_frame_t *frame;
    lv_obj_t *cpe;
} ysw_cpf_t;

ysw_cpf_t *ysw_cpf_create(ysw_cpf_config_t *config);
void ysw_cpf_del(ysw_cpf_t *cpf);
void ysw_cpf_set_cp(ysw_cpf_t *cpf, ysw_cp_t *cp);
void ysw_cpf_redraw(ysw_cpf_t *cpf);
void ysw_cpf_set_header_text(ysw_cpf_t *cpf, const char *header_text);
void ysw_cpf_set_footer_text(ysw_cpf_t *cpf, const char *footer_text);
void ysw_cpf_on_metro(ysw_cpf_t *cpf, note_t *metro_note);
void ysw_cpf_ensure_visible(ysw_cpf_t *cpf, uint32_t first_step_index, uint32_t last_step_index);
