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

lv_style_t ysw_style_plain_color_tight;
lv_style_t ysw_style_pretty_color_tight;
lv_style_t ysw_style_sdb_content;

lv_style_t ysw_style_oi; // cse odd interval
lv_style_t ysw_style_ei; // cse even interval
lv_style_t ysw_style_rn; // cse regular note
lv_style_t ysw_style_sn; // cse selected note
lv_style_t ysw_style_mn; // cse selected note

lv_style_t ysw_style_gray_cell;
lv_style_t ysw_style_white_cell;
lv_style_t ysw_style_yellow_cell;
lv_style_t ysw_style_red_cell;

void ysw_lv_styles_initialize()
{
    lv_style_copy(&ysw_style_plain_color_tight, &lv_style_plain_color);
    ysw_style_plain_color_tight.body.radius = 0;
    ysw_style_plain_color_tight.body.border.width = 0;
    ysw_style_plain_color_tight.body.border.part = LV_BORDER_NONE;
    ysw_style_plain_color_tight.body.padding.top = 0;
    ysw_style_plain_color_tight.body.padding.bottom = 0;
    ysw_style_plain_color_tight.body.padding.left = 0;
    ysw_style_plain_color_tight.body.padding.right = 0;
    ysw_style_plain_color_tight.body.padding.inner = 0;

    lv_style_copy(&ysw_style_pretty_color_tight, &lv_style_pretty_color);
    ysw_style_pretty_color_tight.body.radius = 0;
    ysw_style_pretty_color_tight.body.border.width = 0;
    ysw_style_pretty_color_tight.body.border.part = LV_BORDER_NONE;
    ysw_style_pretty_color_tight.body.padding.top = 0;
    ysw_style_pretty_color_tight.body.padding.bottom = 0;
    ysw_style_pretty_color_tight.body.padding.left = 0;
    ysw_style_pretty_color_tight.body.padding.right = 0;
    ysw_style_pretty_color_tight.body.padding.inner = 0;

    lv_style_copy(&ysw_style_sdb_content, &lv_style_transp);
    ysw_style_sdb_content.body.radius = 0;
    ysw_style_sdb_content.body.border.width = 0;
    ysw_style_sdb_content.body.border.part = LV_BORDER_NONE;
    ysw_style_sdb_content.body.padding.top = 5;
    ysw_style_sdb_content.body.padding.bottom = 0;
    ysw_style_sdb_content.body.padding.left = 5;
    ysw_style_sdb_content.body.padding.right = 0;
    ysw_style_sdb_content.body.padding.inner = 5;

    lv_style_copy(&ysw_style_oi, &lv_style_plain);
    ysw_style_oi.text.font = &lv_font_roboto_12;
    ysw_style_oi.body.main_color = LV_COLOR_SILVER;
    ysw_style_oi.body.grad_color = LV_COLOR_SILVER;
    ysw_style_oi.body.border.color = LV_COLOR_RED;
    ysw_style_oi.body.border.width = 0;
    ysw_style_oi.text.color = LV_COLOR_WHITE;
    ysw_style_oi.text.opa = LV_OPA_COVER;
    ysw_style_oi.line.color = LV_COLOR_BLUE;
    ysw_style_oi.line.width = 1;
    ysw_style_oi.line.opa = LV_OPA_COVER;

    lv_style_copy(&ysw_style_ei, &lv_style_plain);
    ysw_style_ei.text.font = &lv_font_roboto_12;
    ysw_style_ei.body.main_color = LV_COLOR_WHITE;
    ysw_style_ei.body.grad_color = LV_COLOR_WHITE;
    ysw_style_ei.body.border.color = LV_COLOR_RED;
    ysw_style_ei.body.border.width = 0;
    ysw_style_ei.text.color = LV_COLOR_BLACK;
    ysw_style_ei.text.opa =  LV_OPA_COVER;
    ysw_style_ei.line.color = LV_COLOR_BLUE;
    ysw_style_ei.line.width = 1;
    ysw_style_ei.line.opa = LV_OPA_COVER;

    lv_style_copy(&ysw_style_rn, &lv_style_pretty);
    ysw_style_rn.text.font = &lv_font_roboto_12;
    ysw_style_rn.body.main_color = LV_COLOR_RED;
    ysw_style_rn.body.grad_color = LV_COLOR_RED;
    ysw_style_rn.body.opa = LV_OPA_50;

    lv_style_copy(&ysw_style_sn, &lv_style_pretty);
    ysw_style_sn.text.font = &lv_font_roboto_12;
    ysw_style_sn.body.main_color = LV_COLOR_YELLOW;
    ysw_style_sn.body.grad_color = LV_COLOR_YELLOW;
    ysw_style_sn.body.opa = LV_OPA_80;

    lv_style_copy(&ysw_style_mn, &lv_style_pretty);
    ysw_style_mn.text.font = &lv_font_roboto_12;
    ysw_style_mn.line.color = LV_COLOR_RED;
    ysw_style_mn.line.width = 3;
    ysw_style_mn.line.opa = LV_OPA_COVER;
    ysw_style_mn.body.main_color = LV_COLOR_RED;
    ysw_style_mn.body.grad_color = LV_COLOR_RED;
    ysw_style_mn.body.opa = LV_OPA_80;

    lv_style_copy(&ysw_style_gray_cell, &lv_style_plain);
    ysw_style_gray_cell.body.border.width = 1;
    ysw_style_gray_cell.body.border.color = LV_COLOR_BLACK;
    ysw_style_gray_cell.body.main_color = LV_COLOR_SILVER;
    ysw_style_gray_cell.body.grad_color = LV_COLOR_SILVER;

    lv_style_copy(&ysw_style_white_cell, &lv_style_plain);
    ysw_style_white_cell.body.border.width = 1;
    ysw_style_white_cell.body.border.color = LV_COLOR_BLACK;

    lv_style_copy(&ysw_style_yellow_cell, &lv_style_plain);
    ysw_style_yellow_cell.body.border.width = 1;
    ysw_style_yellow_cell.body.border.color = LV_COLOR_BLACK;
    ysw_style_yellow_cell.body.main_color = LV_COLOR_YELLOW;
    ysw_style_yellow_cell.body.grad_color = LV_COLOR_YELLOW;

    lv_style_copy(&ysw_style_red_cell, &lv_style_plain);
    ysw_style_red_cell.body.main_color = LV_COLOR_RED;
    ysw_style_red_cell.body.grad_color = LV_COLOR_RED;
    ysw_style_red_cell.text.color = LV_COLOR_BLACK;
    ysw_style_red_cell.body.border.width = 1;
    ysw_style_red_cell.body.border.color = LV_COLOR_BLACK;
}

