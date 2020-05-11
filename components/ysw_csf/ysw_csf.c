// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_csf.h"

#include "ysw_cs.h"
#include "ysw_csn.h"
#include "ysw_heap.h"
#include "ysw_lv_cse.h"

#include "lvgl/lvgl.h"

#include "esp_log.h"

#define TAG "YSW_CSF"

ysw_csf_t *ysw_csf_create(ysw_csf_config_t *config)
{
    ESP_LOGD(TAG, "ysw_csf_create");

    ysw_csf_t *csf = ysw_heap_allocate(sizeof(ysw_csf_t));
    csf->frame = ysw_frame_create();

    ysw_frame_add_footer_button(csf->frame, LV_SYMBOL_SETTINGS, config->settings_cb);
    ysw_frame_add_footer_button(csf->frame, LV_SYMBOL_SAVE, config->save_cb);
    ysw_frame_add_footer_button(csf->frame, LV_SYMBOL_AUDIO, config->new_cb);
    ysw_frame_add_footer_button(csf->frame, LV_SYMBOL_COPY, config->copy_cb);
    ysw_frame_add_footer_button(csf->frame, LV_SYMBOL_PASTE, config->paste_cb);
    ysw_frame_add_footer_button(csf->frame, LV_SYMBOL_TRASH, config->trash_cb);
    ysw_frame_add_footer_button(csf->frame, LV_SYMBOL_VOLUME_MID, config->volume_mid_cb);
    ysw_frame_add_footer_button(csf->frame, LV_SYMBOL_VOLUME_MAX, config->volume_max_cb);

    ysw_frame_add_header_button(csf->frame, LV_SYMBOL_CLOSE, lv_win_close_event_cb);
    ysw_frame_add_header_button(csf->frame, LV_SYMBOL_NEXT, config->next_cb);
    ysw_frame_add_header_button(csf->frame, LV_SYMBOL_LOOP, config->loop_cb);
    ysw_frame_add_header_button(csf->frame, LV_SYMBOL_PAUSE, config->pause_cb);
    ysw_frame_add_header_button(csf->frame, LV_SYMBOL_PLAY, config->play_cb);
    ysw_frame_add_header_button(csf->frame, LV_SYMBOL_PREV, config->prev_cb);

    csf->cse = ysw_frame_create_content(csf->frame, ysw_lv_cse_create);
    ysw_lv_cse_set_event_cb(csf->cse, config->cse_event_cb);

    return csf;
}

void ysw_csf_free(ysw_csf_t *csf)
{
    ysw_frame_free(csf->frame);
    ysw_heap_free(csf);
}

void ysw_csf_set_cs(ysw_csf_t *csf, ysw_cs_t *cs)
{
    ysw_lv_cse_set_cs(csf->cse, cs);
}

void ysw_csf_redraw(ysw_csf_t *csf)
{
    lv_obj_invalidate(csf->cse);
}

void ysw_csf_set_header_text(ysw_csf_t *csf, const char *header_text)
{
    ysw_frame_set_header_text(csf->frame, header_text);
}

void ysw_csf_set_footer_text(ysw_csf_t *csf, const char *footer_text)
{
    ysw_frame_set_footer_text(csf->frame, footer_text);
}

