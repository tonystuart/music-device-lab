// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "esp_log.h"
#include "ysw_lv_styles.h"

#define TAG "YSW_LV_STYLES"

// Generic Styles are named for what they are rather than what they do
lv_style_t plain_color_tight;

// Specific Styles are named for what they do rather than what they are
lv_style_t cell_editor_style;

lv_style_t value_cell;
lv_style_t win_style_content;
lv_style_t page_bg_style;
lv_style_t page_scrl_style;
lv_style_t name_cell;
lv_style_t selected_cell;

void ysw_lv_styles_initialize()
{
    ESP_LOGD(TAG, "sizeof(style)=%d", sizeof(lv_style_pretty_color));
    lv_style_copy(&page_bg_style, &lv_style_pretty_color);
    ESP_LOGD(TAG, "page_bg_style radius=%d, width=%d, part=%#x, padding top=%d, bottom=%d, left=%d, right=%d, inner=%d", page_bg_style.body.radius, page_bg_style.body.border.width, page_bg_style.body.border.part, page_bg_style.body.padding.top, page_bg_style.body.padding.bottom, page_bg_style.body.padding.left, page_bg_style.body.padding.right, page_bg_style.body.padding.inner);
    page_bg_style.body.radius = 0;
    page_bg_style.body.border.width = 0;
    page_bg_style.body.border.part = LV_BORDER_NONE;
    page_bg_style.body.padding.top = 0;
    page_bg_style.body.padding.bottom = 0;
    page_bg_style.body.padding.left = 0;
    page_bg_style.body.padding.right = 0;
    page_bg_style.body.padding.inner = 0;

    lv_style_copy(&page_scrl_style, &lv_style_pretty_color);
    ESP_LOGD(TAG, "page_scrl_style radius=%d, width=%d, part=%#x, padding top=%d, bottom=%d, left=%d, right=%d, inner=%d", page_scrl_style.body.radius, page_scrl_style.body.border.width, page_scrl_style.body.border.part, page_scrl_style.body.padding.top, page_scrl_style.body.padding.bottom, page_scrl_style.body.padding.left, page_scrl_style.body.padding.right, page_scrl_style.body.padding.inner);
    page_scrl_style.body.radius = 0;
    page_scrl_style.body.border.width = 0;
    page_scrl_style.body.border.part = LV_BORDER_NONE;
    page_scrl_style.body.padding.top = 0;
    page_scrl_style.body.padding.bottom = 0;
    page_scrl_style.body.padding.left = 0;
    page_scrl_style.body.padding.right = 0;
    page_scrl_style.body.padding.inner = 0;

    lv_style_copy(&plain_color_tight, &lv_style_plain_color);
    ESP_LOGD(TAG, "lv_style_plain_color radius=%d, width=%d, part=%#x, padding top=%d, bottom=%d, left=%d, right=%d, inner=%d", plain_color_tight.body.radius, plain_color_tight.body.border.width, plain_color_tight.body.border.part, plain_color_tight.body.padding.top, plain_color_tight.body.padding.bottom, plain_color_tight.body.padding.left, plain_color_tight.body.padding.right, plain_color_tight.body.padding.inner);
    plain_color_tight.body.radius = 0;
    plain_color_tight.body.border.width = 0;
    plain_color_tight.body.border.part = LV_BORDER_NONE;
    plain_color_tight.body.padding.top = 0;
    plain_color_tight.body.padding.bottom = 0;
    plain_color_tight.body.padding.left = 0;
    plain_color_tight.body.padding.right = 0;
    plain_color_tight.body.padding.inner = 0;

    lv_style_copy(&win_style_content, &lv_style_transp);
    ESP_LOGD(TAG, "win_style_content radius=%d, width=%d, part=%#x, padding top=%d, bottom=%d, left=%d, right=%d, inner=%d", win_style_content.body.radius, win_style_content.body.border.width, win_style_content.body.border.part, win_style_content.body.padding.top, win_style_content.body.padding.bottom, win_style_content.body.padding.left, win_style_content.body.padding.right, win_style_content.body.padding.inner);
    win_style_content.body.radius = 0;
    win_style_content.body.border.width = 0;
    win_style_content.body.border.part = LV_BORDER_NONE;
    win_style_content.body.padding.top = 5;
    win_style_content.body.padding.bottom = 0;
    win_style_content.body.padding.left = 5;
    win_style_content.body.padding.right = 0;
    win_style_content.body.padding.inner = 5;

    lv_style_copy(&value_cell, &lv_style_plain);
    value_cell.body.border.width = 1;
    value_cell.body.border.color = LV_COLOR_BLACK;

    lv_style_copy(&cell_editor_style, &lv_style_plain);
    cell_editor_style.body.border.width = 1;
    cell_editor_style.body.border.color = LV_COLOR_BLACK;
    cell_editor_style.body.main_color = LV_COLOR_RED;
    cell_editor_style.body.grad_color = LV_COLOR_RED;
    cell_editor_style.text.color = LV_COLOR_BLACK;

    lv_style_copy(&name_cell, &lv_style_plain);
    name_cell.body.border.width = 1;
    name_cell.body.border.color = LV_COLOR_BLACK;
    name_cell.body.main_color = LV_COLOR_SILVER;
    name_cell.body.grad_color = LV_COLOR_SILVER;

    lv_style_copy(&selected_cell, &lv_style_plain);
    selected_cell.body.border.width = 1;
    selected_cell.body.border.color = LV_COLOR_BLACK;
    selected_cell.body.main_color = LV_COLOR_YELLOW;
    selected_cell.body.grad_color = LV_COLOR_YELLOW;
}

