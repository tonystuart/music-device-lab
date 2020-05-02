// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_csf.h"

#include "esp_log.h"

#include "lvgl/lvgl.h"

#include "ysw_heap.h"
#include "ysw_synthesizer.h"
#include "ysw_ble_synthesizer.h"
#include "ysw_sequencer.h"
#include "ysw_message.h"
#include "ysw_csn.h"
#include "ysw_cs.h"
#include "ysw_music.h"
#include "ysw_lv_styles.h"
#include "ysw_lv_cse.h"

#define TAG "YSW_LV_CSF"

#define BUTTON_SIZE 20

static lv_obj_t *add_header_button(lv_obj_t *win, const void *img_src, lv_event_cb_t event_cb)
{
    lv_obj_t *btn = lv_win_add_btn(win, img_src);
    if (event_cb) {
        lv_obj_set_event_cb(btn, event_cb);
    }
    return btn;
}

static lv_obj_t *add_footer_button(lv_obj_t *footer, const void *img_src, lv_event_cb_t event_cb)
{
    lv_obj_t *editor = lv_obj_get_parent(footer);
    lv_obj_t *win = lv_obj_get_child_back(editor, NULL);
    lv_win_ext_t *ext = lv_obj_get_ext_attr(win);
    lv_obj_t *previous = lv_ll_get_head(&footer->child_ll); // get prev before adding new

    lv_obj_t *btn = lv_btn_create(footer, NULL);

    lv_btn_set_style(btn, LV_BTN_STYLE_REL, ext->style_btn_rel);
    lv_btn_set_style(btn, LV_BTN_STYLE_PR, ext->style_btn_pr);
    lv_obj_set_size(btn, ext->btn_size, ext->btn_size);

    lv_obj_t *img = lv_img_create(btn, NULL);
    lv_obj_set_click(img, false);
    lv_img_set_src(img, img_src);

    if (!previous) {
        lv_obj_align(btn, footer, LV_ALIGN_IN_TOP_LEFT, 0, 0);
    } else {
        lv_obj_align(btn, previous, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
    }

    if (event_cb) {
        lv_obj_set_event_cb(btn, event_cb);
    }

    return btn;
}

ysw_csf_t *ysw_csf_create(ysw_csf_config_t *config)
{
    ysw_csf_t *csf = ysw_heap_allocate(sizeof(ysw_csf_t));

    lv_coord_t display_w = lv_disp_get_hor_res(NULL);
    lv_coord_t display_h = lv_disp_get_ver_res(NULL);
    lv_coord_t footer_h = BUTTON_SIZE + 5 + 5;

    lv_obj_t *editor = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_style(editor, &plain_color_tight);
    lv_obj_set_size(editor, display_w, display_h);

    csf->win = lv_win_create(editor, NULL);
    lv_win_set_style(csf->win, LV_WIN_STYLE_BG, &lv_style_pretty);
    lv_win_set_style(csf->win, LV_WIN_STYLE_CONTENT, &win_style_content);
    lv_win_set_title(csf->win, "Chord Name");
    lv_win_set_btn_size(csf->win, BUTTON_SIZE);
    lv_obj_set_height(csf->win, display_h - footer_h);

    lv_obj_t *footer = lv_obj_create(editor, NULL);
    lv_obj_set_size(footer, display_w, footer_h);
    lv_obj_align(footer, csf->win, LV_ALIGN_OUT_BOTTOM_RIGHT, 5, 5);

    add_footer_button(footer, LV_SYMBOL_SETTINGS, config->settings_cb);
    add_footer_button(footer, LV_SYMBOL_SAVE, config->save_cb);
    add_footer_button(footer, LV_SYMBOL_COPY, config->copy_cb);
    add_footer_button(footer, LV_SYMBOL_PASTE, config->paste_cb);
    add_footer_button(footer, LV_SYMBOL_VOLUME_MID, config->volume_mid_cb);
    add_footer_button(footer, LV_SYMBOL_VOLUME_MAX, config->volume_max_cb);
    add_footer_button(footer, LV_SYMBOL_UP, config->up_cb);
    add_footer_button(footer, LV_SYMBOL_DOWN, config->down_cb);
    add_footer_button(footer, LV_SYMBOL_PLUS, config->plus_cb);
    add_footer_button(footer, LV_SYMBOL_MINUS, config->minus_cb);
    add_footer_button(footer, LV_SYMBOL_LEFT, config->left_cb);
    add_footer_button(footer, LV_SYMBOL_RIGHT, config->right_cb);
    add_footer_button(footer, LV_SYMBOL_TRASH, config->trash_cb);

    add_header_button(csf->win, LV_SYMBOL_CLOSE, lv_win_close_event_cb);
    add_header_button(csf->win, LV_SYMBOL_NEXT, config->next_cb);
    add_header_button(csf->win, LV_SYMBOL_LOOP, config->loop_cb);
    add_header_button(csf->win, LV_SYMBOL_PAUSE, config->pause_cb);
    add_header_button(csf->win, LV_SYMBOL_PLAY, config->play_cb);
    add_header_button(csf->win, LV_SYMBOL_PREV, config->prev_cb);

    lv_obj_t *page = lv_win_get_content(csf->win);

    lv_page_set_style(page, LV_PAGE_STYLE_SCRL, &page_scrl_style);

    csf->cse = ysw_lv_cse_create(csf->win);
    lv_coord_t w = lv_page_get_fit_width(page);
    lv_coord_t h = lv_page_get_fit_height(page);
    lv_obj_set_size(csf->cse, w, h);
    lv_obj_align(csf->cse, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

    ysw_lv_cse_set_event_cb(csf->cse, config->cse_event_cb);
    return csf;
}

void ysw_csf_free(ysw_csf_t *csf)
{
    lv_obj_del(csf->win);
    ysw_heap_free(csf);
}

void ysw_csf_set_cs(ysw_csf_t *csf, ysw_cs_t *cs)
{
    ysw_lv_cse_set_cs(csf->cse, cs);
    ysw_csf_set_header_text(csf, cs->name);
}

void ysw_csf_set_header_text(ysw_csf_t *csf, const char *header_text)
{
    lv_win_set_title(csf->win, header_text);
}

void ysw_csf_redraw(ysw_csf_t *csf)
{
    lv_obj_invalidate(csf->cse);
}

