// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// Chord Progression Editor (CPE)

#include "ysw_lv_cpe.h"

#include "ysw_cp.h"
#include "ysw_lv_styles.h"
#include "ysw_ticks.h"

#include "lvgl.h"

#include "esp_log.h"

#include "math.h"
#include "stdio.h"

#define TAG "YSW_LV_CPE"

#define LV_OBJX_NAME "ysw_lv_cpe"

#define MINIMUM_DRAG 5

static lv_design_cb_t super_design_cb;
static lv_signal_cb_t super_signal_cb;

static char *key_labels[] =
{
    "I",
    "II",
    "III",
    "IV",
    "V",
    "VI",
    "VII",
};

static void draw_line(lv_coord_t x1, lv_coord_t y1, lv_coord_t x2, lv_coord_t y2, const lv_area_t *mask, const lv_style_t *style)
{
    lv_point_t p1 = {
        .x = x1,
        .y = y1,
    };
    lv_point_t p2 = {
        .x = x2,
        .y = y2,
    };
    lv_draw_line(&p1, &p2, mask, style, style->body.border.opa);
}

static void draw_main(lv_obj_t *cpe, const lv_area_t *mask, lv_design_mode_t mode)
{
    super_design_cb(cpe, mask, mode);

    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);

    if (!ext->cp) {
        return;
    }

    // TODO: Factor out code in common with get_step_area (e.g. get_cpe_metrics)

    uint32_t step_count = ysw_cp_get_step_count(ext->cp);

    lv_coord_t h = lv_obj_get_height(cpe);
    lv_coord_t w = lv_obj_get_width(cpe);

    lv_coord_t x = cpe->coords.x1;
    lv_coord_t y = cpe->coords.y1;

    lv_coord_t column_count = step_count > 16 ? 16 : step_count > 0 ? step_count : 1;
    lv_coord_t column_width = w / column_count;
    lv_coord_t row_height = h / 9; // 9 = header + 7 degrees + footer

    lv_coord_t px = x;
    lv_coord_t py = y + row_height;

    lv_coord_t pw = w;
    lv_coord_t ph = h - (2 * row_height);

    ESP_LOGD(TAG, "h=%d, w=%d, ph=%d", h, w, ph);

    draw_line(px, py, px + pw, py, mask, ext->style_ei);
    draw_line(px, py + ph, px + pw, py + ph, mask, ext->style_ei);

    uint32_t measure = 0;

    for (uint32_t i = 0; i < step_count; i++) {
        ysw_step_t *step = ysw_cp_get_step(ext->cp, i);

        lv_coord_t left = px + (i * column_width);

        if (step->flags & YSW_STEP_NEW_MEASURE) {
            measure++;

            lv_area_t measure_heading_area = {
                .x1 = left,
                .y1 = y,
                .x2 = left + column_width,
                .y2 = py,
            };

            lv_area_t measure_heading_mask;

            if (lv_area_intersect(&measure_heading_mask, mask, &measure_heading_area)) {
                lv_point_t offset = {
                    .x = 0,
                    .y = 0, // e.g. to center vertically
                };
                char buffer[32];
                ysw_itoa(measure, buffer, sizeof(buffer));
                lv_draw_label(&measure_heading_area,
                        &measure_heading_mask,
                        ext->style_ei,
                        ext->style_ei->text.opa,
                        buffer,
                        LV_TXT_FLAG_EXPAND | LV_TXT_FLAG_CENTER,
                        &offset,
                        NULL,
                        NULL,
                        LV_BIDI_DIR_LTR);
            }

        }

        if (i) {
            lv_coord_t column_top = step->flags & YSW_STEP_NEW_MEASURE ? y : py;
            draw_line(left, column_top, left, py + ph, mask, ext->style_ei);
        }

        lv_coord_t cell_top = py + ((YSW_MIDI_UNPO - step->degree) * row_height);

        lv_area_t cell_area = {
            .x1 = left,
            .y1 = cell_top,
            .x2 = left + column_width,
            .y2 = cell_top + row_height,
        };

        lv_area_t cell_mask;

        if (lv_area_intersect(&cell_mask, mask, &cell_area)) {

            if (ext->selected_step == step) {
                lv_draw_rect(&cell_area, &cell_mask, ext->style_sn, ext->style_sn->body.opa);
            } else {
                lv_draw_rect(&cell_area, &cell_mask, ext->style_cn, ext->style_cn->body.opa);
            }

            // vertically center the text
            lv_point_t offset = {
                .x = 0,
                .y = ((cell_area.y2 - cell_area.y1) - ext->style_ei->text.font->line_height) / 2
            };

            lv_draw_label(&cell_area,
                    &cell_mask,
                    ext->style_ei,
                    ext->style_ei->text.opa,
                    key_labels[to_index(step->degree)],
                    LV_TXT_FLAG_EXPAND | LV_TXT_FLAG_CENTER,
                    &offset,
                    NULL,
                    NULL,
                    LV_BIDI_DIR_LTR);
        }

        if (ext->selected_step == step) {
            lv_area_t footer_area = {
                .x1 = x,
                .y1 = py + ph,
                .x2 = x + w,
                .y2 = py + ph + row_height,
            };

            lv_area_t footer_mask;

            if (lv_area_intersect(&footer_mask, mask, &footer_area)) {

                // vertically center the text
                lv_point_t offset = {
                    .x = 0,
                    .y = ((footer_area.y2 - footer_area.y1) - ext->style_ei->text.font->line_height) / 2
                };

                lv_draw_label(&footer_area,
                        &footer_mask,
                        ext->style_ei,
                        ext->style_ei->text.opa,
                        step->cs->name,
                        LV_TXT_FLAG_EXPAND | LV_TXT_FLAG_CENTER,
                        &offset,
                        NULL,
                        NULL,
                        LV_BIDI_DIR_LTR);
            }
        }

    }
}

static bool design_cb(lv_obj_t *cpe, const lv_area_t *mask, lv_design_mode_t mode)
{
    bool result = true;
    switch (mode) {
        case LV_DESIGN_COVER_CHK:
            result = super_design_cb(cpe, mask, mode);
            break;
        case LV_DESIGN_DRAW_MAIN:
            draw_main(cpe, mask, mode);
            break;
        case LV_DESIGN_DRAW_POST:
            break;
    }
    return result;
}

static void fire_select(lv_obj_t *cpe, ysw_step_t *step)
{
}

static void fire_deselect(lv_obj_t *cpe, ysw_step_t *step)
{
}

static void fire_create(lv_obj_t *cpe, lv_point_t *point)
{
}

static void fire_drag_end(lv_obj_t *cpe)
{
}

static void get_step_area( lv_obj_t *cpe, uint32_t step_index, lv_area_t *ret_area)
{
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    uint32_t step_count = ysw_cp_get_step_count(ext->cp);
    ysw_step_t *step = ysw_cp_get_step(ext->cp, step_index);

    lv_coord_t h = lv_obj_get_height(cpe);
    lv_coord_t w = lv_obj_get_width(cpe);

    lv_coord_t x = cpe->coords.x1;
    lv_coord_t y = cpe->coords.y1;

    // TODO: Factor out code in common with draw_main (e.g. get_cpe_metrics)

    lv_coord_t column_count = step_count > 16 ? 16 : step_count > 0 ? step_count : 1;
    lv_coord_t column_width = w / column_count;
    lv_coord_t row_height = h / 9; // 9 = header + 7 degrees + footer

    lv_coord_t px = x;
    lv_coord_t py = y + row_height;

    lv_coord_t pw = w;
    lv_coord_t ph = h - (2 * row_height);

    lv_coord_t left = px + (step_index * column_width);
    lv_coord_t cell_top = py + ((YSW_MIDI_UNPO - step->degree) * row_height);

    lv_area_t cell_area = {
        .x1 = left,
        .y1 = cell_top,
        .x2 = left + column_width,
        .y2 = cell_top + row_height,
    };

    if (ret_area) {
        *ret_area = cell_area;
    }
}

static void capture_selection(lv_obj_t *cpe, lv_point_t *point)
{
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    ext->dragging = false;
    ext->selected_step = NULL;
    ext->original_step = (ysw_step_t){};
    ext->original_coords = (lv_area_t){};
    ext->last_click = *point;
    uint32_t step_count = ysw_cp_get_step_count(ext->cp);
    for (uint8_t i = 0; i < step_count; i++) {
        lv_area_t step_area;
        ysw_step_t *step = ysw_cp_get_step(ext->cp, i);
        get_step_area(cpe, i, &step_area);
        if ((step_area.x1 <= point->x && point->x <= step_area.x2) &&
            (step_area.y1 <= point->y && point->y <= step_area.y2)) {
                ext->selected_step = step;
                ext->original_step = *step;
                ext->original_coords = cpe->coords;
                return;
            }
    }
}

static void do_drag(lv_obj_t *cpe, lv_point_t *point)
{
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);

    lv_coord_t x = point->x - ext->last_click.x;
    lv_coord_t y = point->y - ext->last_click.y;

    bool drag_x = abs(x) > MINIMUM_DRAG;
    bool drag_y = abs(y) > MINIMUM_DRAG;

    ext->dragging = drag_x || drag_y;

    if (ext->dragging) {

        ESP_LOGE(TAG, "do_drag dragged x=%d, y=%d from start x=%d, y=%d, step=%p", x, y, ext->last_click.x, ext->last_click.y, ext->selected_step);

        if (ext->selected_step) {
            if (drag_y) {
                // TODO: factor out common metrics
                uint8_t new_degree = ext->original_step.degree - (y / 20);
                if (new_degree >= 1 && new_degree <= YSW_MIDI_UNPO) {
                    ext->selected_step->degree = new_degree;
                }
            }
        } else {
            if (drag_x) {
                lv_coord_t new_x1 = ext->original_coords.x1 + x;
                cpe->coords.x1 = new_x1;
            }
        }
    }
}

static void do_click(lv_obj_t *cpe)
{
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    if (ext->long_press) {
        ext->long_press = false;
    }
}

static void on_signal_pressed(lv_obj_t *cpe, void *param)
{
    lv_indev_t *indev_act = (lv_indev_t*)param;
    lv_indev_proc_t *proc = &indev_act->proc;
    lv_point_t *point = &proc->types.pointer.act_point;
    capture_selection(cpe, point);
}

static void on_signal_pressing(lv_obj_t *cpe, void *param)
{
    lv_indev_t *indev_act = (lv_indev_t*)param;
    lv_indev_proc_t *proc = &indev_act->proc;
    do_drag(cpe, &proc->types.pointer.act_point);
    lv_obj_invalidate(cpe);
}

static void on_signal_released(lv_obj_t *cpe, void *param)
{
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    lv_indev_t *indev_act = (lv_indev_t*)param;
    lv_indev_proc_t *proc = &indev_act->proc;
    do_drag(cpe, &proc->types.pointer.act_point);
    if (ext->dragging) {
        fire_drag_end(cpe);
        ext->dragging = false;
    } else {
        do_click(cpe);
    }
    lv_obj_invalidate(cpe);
}

static void on_signal_press_lost(lv_obj_t *cpe, void *param)
{
    ESP_LOGD(TAG, "on_signal_press_lost entered");
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    if (ext->dragging) {
        ext->dragging = false;
        if (ext->selected_step) {
            // could save original on drag and set back
        }
    }
    lv_obj_invalidate(cpe);
}

static void on_signal_long_press(lv_obj_t *cpe, void *param) {
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    lv_indev_t *indev_act = (lv_indev_t*)param;
    lv_indev_proc_t *proc = &indev_act->proc;
    lv_point_t *point = &proc->types.pointer.act_point;
    do_drag(cpe, point);
    if (!ext->dragging) {
        if (ext->selected_step) {
            bool selected = !ysw_step_is_selected(ext->selected_step);
            ysw_step_select(ext->selected_step, selected);
            if (selected) {
                fire_select(cpe, ext->selected_step);
            } else {
                fire_deselect(cpe, ext->selected_step);
            }
        } else {
            fire_create(cpe, point);
        }
        ext->long_press = true;
    }
}

static lv_res_t signal_cb(lv_obj_t *cpe, lv_signal_t signal, void *param)
{
    lv_res_t res = super_signal_cb(cpe, signal, param);
    //ESP_LOGD(TAG, "signal_cb signal=%d", signal);
    if (res == LV_RES_OK) {
        switch (signal) {
            case LV_SIGNAL_GET_TYPE:
                res = lv_obj_handle_get_type_signal(param, LV_OBJX_NAME);
                break;
            case LV_SIGNAL_CLEANUP:
                // free anything we allocated
                // note that lv_obj_del frees ext_attr if set
                break;
            case LV_SIGNAL_PRESSED:
                on_signal_pressed(cpe, param);
                break;
            case LV_SIGNAL_PRESSING:
                on_signal_pressing(cpe, param);
                break;
            case LV_SIGNAL_RELEASED:
                on_signal_released(cpe, param);
                break;
            case LV_SIGNAL_PRESS_LOST:
                on_signal_press_lost(cpe, param);
                break;
            case LV_SIGNAL_LONG_PRESS:
                on_signal_long_press(cpe, param);
                break;
        }
    }
    return res;
}

lv_obj_t *ysw_lv_cpe_create(lv_obj_t *par)
{
    lv_obj_t *cpe = lv_obj_create(par, NULL);
    LV_ASSERT_MEM(cpe);
    if (cpe == NULL) {
        return NULL;
    }

    if (super_signal_cb == NULL) {
        super_signal_cb = lv_obj_get_signal_cb(cpe);
    }

    if (super_design_cb == NULL) {
        super_design_cb = lv_obj_get_design_cb(cpe);
    }

    ysw_lv_cpe_ext_t *ext = lv_obj_allocate_ext_attr(cpe, sizeof(ysw_lv_cpe_ext_t));
    LV_ASSERT_MEM(ext);
    if (ext == NULL) {
        return NULL;
    }

    ext->cp = NULL;
    ext->selected_step = NULL;
    ext->original_step = (ysw_step_t){};
    ext->original_coords = (lv_area_t){};

    ext->style_bg = &lv_style_plain;
    ext->style_oi = &odd_interval_style;
    ext->style_ei = &even_interval_style;
    ext->style_cn = &cn_style;
    ext->style_sn = &selected_cn_style;

    ext->event_cb = NULL;

    lv_obj_set_signal_cb(cpe, signal_cb);
    lv_obj_set_design_cb(cpe, design_cb);
    lv_obj_set_click(cpe, true);
    lv_obj_set_style(cpe, &lv_style_plain);

    return cpe;
}

void ysw_lv_cpe_set_cp(lv_obj_t *cpe, ysw_cp_t *cp)
{
    LV_ASSERT_OBJ(cpe, LV_OBJX_NAME);
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    if (ext->cp == cp) {
        return;
    }
    ext->cp = cp;
    lv_obj_invalidate(cpe);
}

void ysw_lv_cpe_set_event_cb(lv_obj_t *cpe, ysw_lv_cpe_event_cb_t event_cb)
{
    LV_ASSERT_OBJ(cpe, LV_OBJX_NAME);
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    ext->event_cb = event_cb;
}
