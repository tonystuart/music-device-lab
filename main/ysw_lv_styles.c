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
lv_style_t white_key_style;
lv_style_t black_key_style;
lv_style_t cell_editor_style;

lv_style_t value_cell;
lv_style_t win_style_content;
lv_style_t page_bg_style;
lv_style_t page_scrl_style;
lv_style_t name_cell;
lv_style_t selected_cell;

void log_style_metrics(lv_style_t *style, char *tag)
{
    ESP_LOGD(TAG, "sizeof(lv_style_t)=%d", sizeof(lv_style_t));
    ESP_LOGD(tag, "radius=%d, width=%d, part=%#x, padding top=%d, bottom=%d, left=%d, right=%d, inner=%d", style->body.radius, style->body.border.width, style->body.border.part, style->body.padding.top, style->body.padding.bottom, style->body.padding.left, style->body.padding.right, style->body.padding.inner);
}

void ysw_lv_styles_initialize()
{
    lv_style_copy(&black_key_style, &lv_style_plain);
    black_key_style.text.font = &lv_font_roboto_12;
    black_key_style.body.main_color = LV_COLOR_SILVER;
    black_key_style.body.grad_color = LV_COLOR_SILVER;
    black_key_style.body.border.color = LV_COLOR_RED;
    black_key_style.body.border.width = 0;
    black_key_style.text.color = LV_COLOR_WHITE;
    black_key_style.text.opa = LV_OPA_COVER;
    black_key_style.line.color = LV_COLOR_BLUE;
    black_key_style.line.width = 1;
    black_key_style.line.opa = LV_OPA_COVER;

    lv_style_copy(&white_key_style, &lv_style_plain);
    white_key_style.text.font = &lv_font_roboto_12;
    white_key_style.body.main_color = LV_COLOR_WHITE;
    white_key_style.body.grad_color = LV_COLOR_WHITE;
    white_key_style.body.border.color = LV_COLOR_RED;
    white_key_style.body.border.width = 0;
    white_key_style.text.color = LV_COLOR_BLACK;
    white_key_style.text.opa =  LV_OPA_COVER;
    white_key_style.line.color = LV_COLOR_BLUE;
    white_key_style.line.width = 1;
    white_key_style.line.opa = LV_OPA_COVER;

    lv_style_copy(&page_bg_style, &lv_style_pretty_color);
    page_bg_style.body.radius = 0;
    page_bg_style.body.border.width = 0;
    page_bg_style.body.border.part = LV_BORDER_NONE;
    page_bg_style.body.padding.top = 0;
    page_bg_style.body.padding.bottom = 0;
    page_bg_style.body.padding.left = 0;
    page_bg_style.body.padding.right = 0;
    page_bg_style.body.padding.inner = 0;

    lv_style_copy(&page_scrl_style, &lv_style_pretty_color);
    page_scrl_style.body.radius = 0;
    page_scrl_style.body.border.width = 0;
    page_scrl_style.body.border.part = LV_BORDER_NONE;
    page_scrl_style.body.padding.top = 0;
    page_scrl_style.body.padding.bottom = 0;
    page_scrl_style.body.padding.left = 0;
    page_scrl_style.body.padding.right = 0;
    page_scrl_style.body.padding.inner = 0;

    lv_style_copy(&plain_color_tight, &lv_style_plain_color);
    plain_color_tight.body.radius = 0;
    plain_color_tight.body.border.width = 0;
    plain_color_tight.body.border.part = LV_BORDER_NONE;
    plain_color_tight.body.padding.top = 0;
    plain_color_tight.body.padding.bottom = 0;
    plain_color_tight.body.padding.left = 0;
    plain_color_tight.body.padding.right = 0;
    plain_color_tight.body.padding.inner = 0;

    lv_style_copy(&win_style_content, &lv_style_transp);
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

