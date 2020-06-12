// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_frame.h"

#include "ysw_heap.h"
#include "ysw_lv_styles.h"

#include "lvgl/lvgl.h"

#include "esp_log.h"

#define TAG "YSW_FRAME"

#define BUTTON_SIZE 20
#define BUTTON_PAD 5

// Key to ysw_frame context management:
// We stash the context in the header and footer
// We stash the callback in each button's user data
// We fetch the context using the parent relationship

static void on_btn_event(lv_obj_t *btn, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        ysw_frame_cb_t cb = lv_obj_get_user_data(btn);
        if (cb) {
            lv_obj_t *parent = lv_obj_get_parent(btn);
            void *context = lv_obj_get_user_data(parent);
            cb(context, btn);
        }
    }
}

ysw_frame_t *ysw_frame_create(void *context)
{
    ESP_LOGD(TAG, "ysw_frame_create");

    ysw_frame_t *frame = ysw_heap_allocate(sizeof(ysw_frame_t));

    lv_coord_t display_w = lv_disp_get_hor_res(NULL);
    lv_coord_t display_h = lv_disp_get_ver_res(NULL);
    lv_coord_t footer_h = BUTTON_SIZE + BUTTON_PAD + BUTTON_PAD;

    frame->container = lv_obj_create(lv_scr_act(), NULL);
    //v7: lv_obj_set_style(frame->container, &ysw_style_none);
    lv_obj_set_size(frame->container, display_w, display_h);

    frame->win = lv_win_create(frame->container, NULL);
    //v7: lv_win_set_style(frame->win, LV_WIN_STYLE_BG, &ysw_style_none);
    // NB: LV_WIN_STYLE_CONTENT and LV_PAGE_STYLE_SCRL are equivalent

    //afs: see if this works:
    lv_obj_set_size(frame->win, display_w, display_h);

    lv_obj_t *page = lv_win_get_content(frame->win);
    //v7: lv_page_set_style(page, LV_PAGE_STYLE_BG, &ysw_style_none);
    //v7: lv_page_set_style(page, LV_PAGE_STYLE_SCRL, &ysw_style_none); // e.g. scroll area below short table

    lv_win_set_title(frame->win, "");
    //v7: lv_win_set_btn_size(frame->win, BUTTON_SIZE);
    lv_obj_set_height(frame->win, display_h - footer_h);
    lv_win_ext_t *ext = lv_obj_get_ext_attr(frame->win);

    //afs: see if this works:
    lv_obj_set_height(ext->header, footer_h);

    lv_obj_set_user_data(ext->header, context);

    frame->footer = lv_obj_create(frame->container, NULL);
    lv_obj_set_size(frame->footer, display_w, footer_h);
    lv_obj_align(frame->footer, frame->win, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
    lv_obj_set_user_data(frame->footer, context);

    frame->footer_label = lv_label_create(frame->footer, NULL);
    lv_label_set_text(frame->footer_label, "");
    lv_obj_align(frame->footer_label, frame->footer, LV_ALIGN_IN_TOP_RIGHT, -BUTTON_PAD, BUTTON_PAD);

    return frame;
}

lv_obj_t *ysw_frame_add_header_button(ysw_frame_t *frame, const void *img_src, void *cb)
{
    lv_obj_t *btn = lv_win_add_btn(frame->win, img_src);
    lv_obj_set_user_data(btn, cb);
    lv_obj_set_event_cb(btn, on_btn_event);
    return btn;
}

lv_obj_t *ysw_frame_add_footer_button(ysw_frame_t *frame, const void *img_src, void *cb)
{
    lv_obj_t *win = lv_obj_get_child_back(frame->container, NULL);
    lv_win_ext_t *ext = lv_obj_get_ext_attr(win);

    lv_obj_t *btn = lv_btn_create(frame->footer, NULL);

    //v7: lv_btn_set_style(btn, LV_BTN_STYLE_REL, ext->style_btn_rel);
    //v7: lv_btn_set_style(btn, LV_BTN_STYLE_PR, ext->style_btn_pr);
    lv_coord_t btn_size = lv_obj_get_height_fit(ext->header); //v7
    lv_obj_set_size(btn, btn_size, btn_size); //v7

    lv_obj_t *img = lv_img_create(btn, NULL);
    lv_obj_set_click(img, false);
    lv_img_set_src(img, img_src);

    if (!frame->last_button) {
        lv_obj_align(btn, frame->footer, LV_ALIGN_IN_TOP_LEFT, BUTTON_PAD, BUTTON_PAD);
    } else {
        lv_obj_align(btn, frame->last_button, LV_ALIGN_OUT_RIGHT_MID, BUTTON_PAD, 0); // 0 from previous y
    }

    lv_obj_set_user_data(btn, cb);
    lv_obj_set_event_cb(btn, on_btn_event);

    frame->last_button = btn;
    return btn;
}

void ysw_frame_set_content(ysw_frame_t *frame, lv_obj_t *object)
{
    ESP_LOGD(TAG, "ysw_frame_set_content");
    lv_obj_t *page = lv_win_get_content(frame->win);
    lv_coord_t w = lv_page_get_width_fit(page);
    lv_coord_t h = lv_page_get_height_fit(page);
    lv_obj_set_size(object, w, h);
    lv_obj_align(object, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);
}

void ysw_frame_del(ysw_frame_t *frame)
{
    lv_obj_del(frame->container);
    ysw_heap_free(frame);
}

void ysw_frame_set_header_text(ysw_frame_t *frame, const char *header_text) {
    lv_win_set_title(frame->win, header_text);
}

void ysw_frame_set_footer_text(ysw_frame_t *frame, const char *footer_text)
{
    lv_label_set_text(frame->footer_label, footer_text);
    lv_obj_realign(frame->footer_label);
}

