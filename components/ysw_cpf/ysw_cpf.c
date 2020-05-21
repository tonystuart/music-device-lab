// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_cpf.h"

#include "ysw_cn.h"
#include "ysw_cp.h"
#include "ysw_cs.h"
#include "ysw_heap.h"
#include "ysw_lv_cpe.h"

#include "lvgl/lvgl.h"

#include "esp_log.h"

#define TAG "YSW_CPF"

ysw_cpf_t *ysw_cpf_create(ysw_cpf_config_t *config)
{
    ESP_LOGD(TAG, "ysw_cpf_create");

    ysw_cpf_t *cpf = ysw_heap_allocate(sizeof(ysw_cpf_t));
    cpf->frame = ysw_frame_create();

    ysw_frame_add_footer_button(cpf->frame, LV_SYMBOL_SETTINGS, config->settings_cb);
    ysw_frame_add_footer_button(cpf->frame, LV_SYMBOL_SAVE, config->save_cb);
    ysw_frame_add_footer_button(cpf->frame, LV_SYMBOL_AUDIO, config->new_cb);
    ysw_frame_add_footer_button(cpf->frame, LV_SYMBOL_COPY, config->copy_cb);
    ysw_frame_add_footer_button(cpf->frame, LV_SYMBOL_PASTE, config->paste_cb);
    ysw_frame_add_footer_button(cpf->frame, LV_SYMBOL_TRASH, config->trash_cb);
    ysw_frame_add_footer_button(cpf->frame, LV_SYMBOL_LEFT, config->left_cb);
    ysw_frame_add_footer_button(cpf->frame, LV_SYMBOL_RIGHT, config->right_cb);

    ysw_frame_add_header_button(cpf->frame, LV_SYMBOL_CLOSE, config->close_cb);
    ysw_frame_add_header_button(cpf->frame, LV_SYMBOL_NEXT, config->next_cb);
    ysw_frame_add_header_button(cpf->frame, LV_SYMBOL_LOOP, config->loop_cb);
    ysw_frame_add_header_button(cpf->frame, LV_SYMBOL_PAUSE, config->pause_cb);
    ysw_frame_add_header_button(cpf->frame, LV_SYMBOL_PLAY, config->play_cb);
    ysw_frame_add_header_button(cpf->frame, LV_SYMBOL_PREV, config->prev_cb);

    cpf->cpe = ysw_lv_cpe_create(cpf->frame->win);
    ysw_frame_set_content(cpf->frame, cpf->cpe);
    ysw_lv_cpe_set_event_cb(cpf->cpe, config->cpe_event_cb);

    return cpf;
}

void ysw_cpf_del(ysw_cpf_t *cpf)
{
    ysw_frame_del(cpf->frame);
    ysw_heap_free(cpf);
}

void ysw_cpf_set_cp(ysw_cpf_t *cpf, ysw_cp_t *cp)
{
    ysw_lv_cpe_set_cp(cpf->cpe, cp);
}

void ysw_cpf_redraw(ysw_cpf_t *cpf)
{
    lv_obj_invalidate(cpf->cpe);
}

void ysw_cpf_set_header_text(ysw_cpf_t *cpf, const char *header_text)
{
    ysw_frame_set_header_text(cpf->frame, header_text);
}

void ysw_cpf_set_footer_text(ysw_cpf_t *cpf, const char *footer_text)
{
    ysw_frame_set_footer_text(cpf->frame, footer_text);
}

