// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_ui.h"
#include "ysw_main_seq.h"

#include "lvgl.h"
#include "esp_log.h"

#define TAG "YSW_UI"

void ysw_ui_distribute_extra_width(lv_obj_t *parent, lv_obj_t *obj)
{
    lv_coord_t inner = lv_obj_get_style_pad_inner(parent, LV_CONT_PART_MAIN);

    lv_obj_t *child;
    lv_coord_t width = lv_obj_get_style_pad_left(parent, LV_CONT_PART_MAIN);
    ;
    _LV_LL_READ_BACK(parent->child_ll, child)
    {
        lv_style_int_t mleft = lv_obj_get_style_margin_left(child, LV_OBJ_PART_MAIN);
        lv_style_int_t mright = lv_obj_get_style_margin_right(child, LV_OBJ_PART_MAIN);
        width += lv_obj_get_width(child) + inner + mleft + mright;
    }
    ESP_LOGD(TAG, "extra_width=%d", width);
    lv_coord_t parent_width = lv_obj_get_width(parent);
    lv_coord_t extra_width = parent_width - width; // can be negative
    lv_obj_set_width(obj, lv_obj_get_width(obj) + extra_width);
}

void ysw_ui_distribute_extra_height(lv_obj_t *parent, lv_obj_t *obj)
{
    lv_coord_t inner = lv_obj_get_style_pad_inner(parent, LV_CONT_PART_MAIN);

    lv_obj_t *child;
    lv_coord_t height = lv_obj_get_style_pad_top(parent, LV_CONT_PART_MAIN);
    ;
    _LV_LL_READ_BACK(parent->child_ll, child)
    {
        lv_style_int_t mtop = lv_obj_get_style_margin_top(child, LV_OBJ_PART_MAIN);
        lv_style_int_t mbottom = lv_obj_get_style_margin_bottom(child, LV_OBJ_PART_MAIN);
        height += lv_obj_get_height(child) + inner + mtop + mbottom;
    }
    ESP_LOGD(TAG, "extra_height=%d", height);
    lv_coord_t parent_height = lv_obj_get_height(parent);
    lv_coord_t extra_height = parent_height - height; // can be negative
    lv_obj_set_height(obj, lv_obj_get_height(obj) + extra_height);
}

void ysw_ui_get_obj_type(lv_obj_t *obj, char *buffer, uint32_t size)
{
    lv_obj_type_t types;
    lv_obj_get_type(obj, &types);

    char *t = buffer;
    char *t_max = buffer + size - 1; // -1 for null terminator

    uint8_t i;
    bool done = false;
    for (i = 0; i < LV_MAX_ANCESTOR_NUM && !done; i++) {
        if (!types.type[i]) {
            done = true;
        } else {
            if (t >= t_max) {
                done = true;
            } else {
                *t++ = '/';
            }
            const char *s = types.type[i];
            while (*s && !done) {
                if (t >= t_max) {
                    done = true;
                } else {
                    *t++ = *s++;
                }
            }
        }
    }
    *t = 0;
}

int32_t ysw_ui_get_index_of_child(lv_obj_t *obj)
{
    lv_obj_t *parent = lv_obj_get_parent(obj);
    if (parent) {
        int32_t index = 0;
        lv_obj_t *child;
        _LV_LL_READ_BACK(parent->child_ll, child)
        {
            if (child == obj) {
                return index;
            }
            index++;
        }
    }
    return -1;
}

lv_obj_t* ysw_ui_child_at_index(lv_obj_t *parent, uint32_t index)
{
    uint32_t current = 0;
    lv_obj_t *child;
    _LV_LL_READ_BACK(parent->child_ll, child)
    {
        if (current == index) {
            return child;
        }
        current++;
    }
    return NULL;
}

void ysw_ui_ensure_visible(lv_obj_t *child, bool do_center)
{
    lv_obj_t *scrl = lv_obj_get_parent(child);
    lv_coord_t scrl_top = lv_obj_get_y(scrl); // always <= 0

    lv_obj_t *viewport = lv_obj_get_parent(scrl);
    lv_coord_t viewport_height = lv_obj_get_height(viewport);

    lv_coord_t child_height = lv_obj_get_height(child);
    lv_coord_t child_top = lv_obj_get_y(child);
    lv_coord_t child_bottom = child_top + child_height;

    lv_coord_t visible_top = -scrl_top;
    lv_coord_t visible_bottom = visible_top + viewport_height;

    bool is_above = child_top < visible_top;
    bool is_below = child_bottom > visible_bottom;

    if (is_above || is_below) {
        if (do_center || (is_above && is_below)) {
            lv_coord_t center_offset = (viewport_height - child_height) / 2;
            scrl_top = -child_top + center_offset;
        } else if (is_above) {
            scrl_top = -child_top;
        } else if (is_below) {
            lv_coord_t bottom_offset = (viewport_height - child_height);
            scrl_top = -child_top + bottom_offset;
        }
        lv_obj_set_y(scrl, scrl_top);
    }
}

void ysw_ui_on_btn_event(lv_obj_t *btn, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        ysw_ui_cbd_t *cbd = lv_obj_get_user_data(btn);
        if (cbd) {
            cbd->cb(cbd->context, btn);
        }
    }
}

void ysw_ui_init_cbd(ysw_ui_cbd_t *cbd, void *cb, void *context)
{
    cbd->cb = cb;
    cbd->context = context;
}

void ysw_ui_init_button(ysw_ui_button_t *button, void *img_src, ysw_ui_btn_cb_t cb, void *context)
{
    button->img_src = img_src;
    ysw_ui_init_cbd(&button->cbd, cb, context);
}

void ysw_ui_init_buttons(ysw_ui_button_t buttons[], const ysw_ui_btn_def_t defs[], void *context)
{
    for (uint32_t i = 0; defs[i].img_src && i < YSW_UI_BUTTONS; i++) {
        ysw_ui_init_button(&buttons[i], defs[i].img_src, defs[i].btn_cb, context);
    }
}

void ysw_ui_create_label(lv_obj_t *parent, ysw_ui_label_t *label)
{
    label->label = lv_label_create(parent, NULL);
    lv_obj_set_size(label->label, 0, 30);
    lv_label_set_long_mode(label->label, LV_LABEL_LONG_CROP);
}

void ysw_ui_create_button(lv_obj_t *parent, ysw_ui_button_t *button)
{
    button->btn = lv_btn_create(parent, NULL);
    lv_obj_set_size(button->btn, 20, 20);
    ysw_style_adjust_obj(button->btn);
    lv_obj_t *img = lv_img_create(button->btn, NULL);
    lv_obj_set_click(img, false);
    lv_img_set_src(img, button->img_src);
    lv_obj_set_user_data(button->btn, &button->cbd);
    lv_obj_set_event_cb(button->btn, ysw_ui_on_btn_event);
    if (strcmp(button->img_src, LV_SYMBOL_LOOP) == 0) {
        ysw_main_seq_init_loop_btn(button->btn);
    }
}

void ysw_ui_create_header(lv_obj_t *parent, ysw_ui_header_t *header)
{
    header->container = lv_cont_create(parent, NULL);
    lv_obj_set_size(header->container, 310, 30);
    ysw_style_adjust_obj(header->container);
    ysw_ui_create_label(header->container, &header->title);

    for (uint32_t i = 0; i < YSW_UI_BUTTONS; i++) {
        if (header->buttons[i].img_src) {
            ysw_ui_create_button(header->container, &header->buttons[i]);
        }
    }

    ysw_ui_distribute_extra_width(header->container, header->title.label);
    lv_cont_set_layout(header->container, LV_LAYOUT_ROW_MID);
}

void ysw_ui_set_header_text(ysw_ui_header_t *header, const char *text)
{
    lv_label_set_text(header->title.label, text);
}

void ysw_ui_create_body(lv_obj_t *parent, ysw_ui_body_t *body)
{
    body->page = lv_page_create(parent, NULL);
    lv_obj_set_size(body->page, 310, 0);
    ysw_style_adjust_obj(body->page);
    ysw_style_adjust_obj(lv_page_get_scrl(body->page));
    lv_page_set_scrl_layout(body->page, LV_LAYOUT_COLUMN_MID);
}

void ysw_ui_create_footer(lv_obj_t *parent, ysw_ui_footer_t *footer)
{
    footer->container = lv_cont_create(parent, NULL);
    lv_obj_set_size(footer->container, 310, 30);
    ysw_style_adjust_obj(footer->container);

    for (uint32_t i = 0; i < YSW_UI_BUTTONS; i++) {
        if (footer->buttons[i].img_src) {
            ysw_ui_create_button(footer->container, &footer->buttons[i]);
        }
    }

    ysw_ui_create_label(footer->container, &footer->info);
    lv_label_set_align(footer->info.label, LV_LABEL_ALIGN_RIGHT);

    ysw_ui_distribute_extra_width(footer->container, footer->info.label);
    lv_cont_set_layout(footer->container, LV_LAYOUT_ROW_MID);
}

void ysw_ui_set_footer_text(ysw_ui_footer_t *footer, const char *text)
{
    lv_label_set_text(footer->info.label, text);
}

void ysw_ui_create_frame(ysw_ui_frame_t *frame, lv_obj_t *parent)
{
    frame->container = lv_cont_create(parent, NULL);
    lv_obj_set_size(frame->container, 320, 240);
    ysw_style_adjust_obj(frame->container);
    lv_obj_align_origo(frame->container, parent, LV_ALIGN_CENTER, 0, 0);

    ysw_ui_create_header(frame->container, &frame->header);
    ysw_ui_create_body(frame->container, &frame->body);

    if (frame->footer.buttons[0].img_src) {
        ysw_ui_create_footer(frame->container, &frame->footer);
    }

    ysw_ui_distribute_extra_height(frame->container, frame->body.page);
    lv_cont_set_layout(frame->container, LV_LAYOUT_COLUMN_MID);
}

void ysw_ui_close_frame(ysw_ui_frame_t *frame)
{
    lv_obj_del(frame->container);
}
