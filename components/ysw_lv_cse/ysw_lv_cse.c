// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// Chord Style Editor (CSE)

#include "ysw_lv_cse.h"

#include "ysw_cs.h"
#include "ysw_cn.h"
#include "ysw_lv_styles.h"
#include "ysw_ticks.h"

#include "lvgl.h"

#include "esp_log.h"

#include "math.h"
#include "stdio.h"

#define TAG "YSW_LV_CSE"

#define LV_OBJX_NAME "ysw_lv_cse"

static lv_design_cb_t super_design_cb;
static lv_signal_cb_t super_signal_cb;

// Note that rows are drawn top down in reverse logical order

static char *key_labels[] =
{
    "7th",
    NULL,
    "5th",
    NULL,
    "3rd",
    NULL,
    "1st",
};

#define ROW_COUNT YSW_MIDI_UNPO

#define MINIMUM_DRAG 5

#define MINIMUM_DURATION 10

static void get_cn_info(
        lv_obj_t *cse,
        ysw_cn_t *cn,
        lv_area_t *ret_area,
        uint8_t *ret_degree,
        int8_t *ret_octave)
{
    uint8_t degree;
    int8_t octave;

    ysw_degree_normalize(cn->degree, &degree, &octave);

    lv_coord_t h = lv_obj_get_height(cse);
    lv_coord_t w = lv_obj_get_width(cse);

    lv_coord_t x = cse->coords.x1;
    lv_coord_t y = cse->coords.y1;

    lv_coord_t delta_y = -ysw_cn_get_accidental(cn) * ((h / ROW_COUNT) / 2);

    lv_coord_t x1 = x + (cn->start * w) / YSW_CS_DURATION;
    lv_coord_t x2 = x + ((cn->start + cn->duration) * w) / YSW_CS_DURATION;

    lv_coord_t y1 = y + (((YSW_MIDI_UNPO - degree) * h) / ROW_COUNT) + delta_y;
    lv_coord_t y2 = y + ((((YSW_MIDI_UNPO - degree) + 1) * h) / ROW_COUNT) + delta_y;

    lv_area_t area = {
        .x1 = x1,
        .y1 = y1 + 4,
        .x2 = x2,
        .y2 = y2 - 4
    };

    if (ret_area) {
        *ret_area = area;
    }

    if (ret_degree) {
        *ret_degree = degree;
    }

    if (ret_octave) {
        *ret_octave = octave;
    }
}

static void draw_main(lv_obj_t *cse, const lv_area_t *mask, lv_design_mode_t mode)
{
    super_design_cb(cse, mask, mode);

    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);

    if (!ext->cs) {
        return;
    }

    lv_coord_t h = lv_obj_get_height(cse);
    lv_coord_t w = lv_obj_get_width(cse);

    lv_coord_t x = cse->coords.x1;
    lv_coord_t y = cse->coords.y1;

    for (lv_coord_t i = 0; i < ROW_COUNT; i++) {

        lv_area_t row_area = {
            .x1 = x,
            .y1 = y + ((i * h) / ROW_COUNT),
            .x2 = x + w,
            .y2 = y + (((i + 1) * h) / ROW_COUNT)
        };

        lv_area_t row_mask;
        if (lv_area_intersect(&row_mask, mask, &row_area)) {

            ESP_LOGD(TAG, "calling draw_rect");
            if (i & 0x01) {
                lv_draw_rect(&row_area, &row_mask, ext->oi_style, ext->oi_style->body.opa);
            } else {
                lv_draw_rect(&row_area, &row_mask, ext->ei_style, ext->ei_style->body.opa);

                lv_area_t label_mask;
                lv_area_t label_area = {
                    .x1 = row_area.x1,
                    .y1 = row_area.y1,
                    .x2 = row_area.x1 + 50,
                    .y2 = row_area.y1 + 15
                };
                if (lv_area_intersect(&label_mask, mask, &label_area)) {
                    lv_draw_label(&label_area,
                            &label_mask,
                            ext->ei_style,
                            ext->ei_style->text.opa,
                            key_labels[i],
                            LV_TXT_FLAG_EXPAND,
                            NULL,
                            NULL,
                            NULL,
                            LV_BIDI_DIR_LTR);
                }
            }
            ESP_LOGD(TAG, "later");

            if (i) {
                lv_point_t point1 = {
                    .x = row_area.x1,
                    .y = row_area.y1
                };
                lv_point_t point2 = {
                    .x = row_area.x1 + w,
                    .y = row_area.y1
                };
                if (i & 0x01) {
                    lv_draw_line(&point1, &point2, mask, ext->oi_style, ext->oi_style->body.border.opa);
                } else {
                    lv_draw_line(&point1, &point2, mask, ext->ei_style, ext->ei_style->body.border.opa);
                }
            }
        }
    }

    for (int i = 0; i < ext->cs->divisions; i++) {
        lv_point_t point1 = {
            .x = x + ((i * w) / ext->cs->divisions),
            .y = y
        };
        lv_point_t point2 = {
            .x = x + ((i * w) / ext->cs->divisions),
            .y = y + h
        };
        lv_draw_line(&point1, &point2, mask, ext->ei_style, ext->ei_style->body.border.opa);
    }

    uint32_t cn_count = ysw_cs_get_cn_count(ext->cs);

    for (int i = 0; i < cn_count; i++) {

        ysw_cn_t *cn = ysw_cs_get_cn(ext->cs, i);

        int8_t octave = 0;
        lv_area_t cn_area = {};
        get_cn_info(cse, cn, &cn_area, NULL, &octave);

        lv_area_t cn_mask;

        if (lv_area_intersect(&cn_mask, mask, &cn_area)) {

            if (ysw_cn_is_selected(cn)) {
                // TODO: remove block if we don't use a separate style for dragging
                if (ext->dragging) {
                    lv_draw_rect(&cn_area, &cn_mask, ext->sn_style, ext->sn_style->body.opa);
                } else {
                    lv_draw_rect(&cn_area, &cn_mask, ext->sn_style, ext->sn_style->body.opa);
                }
            } else {
                lv_draw_rect(&cn_area, &cn_mask, ext->rn_style, ext->rn_style->body.opa);
            }

            char buffer[32];
            if (octave) {
                snprintf(buffer, sizeof(buffer), "%d/%+d", cn->velocity, octave);
            } else {
                snprintf(buffer, sizeof(buffer), "%d", cn->velocity);
            }

            // vertically center the text
            lv_point_t offset = {
                .x = 0,
                .y = ((cn_area.y2 - cn_area.y1) - ext->rn_style->text.font->line_height) / 2
            };

            if (ysw_cn_is_selected(cn)) {
                // TODO: remove block if we don't use a separate style for dragging
                if (ext->dragging) {
                    lv_draw_label(&cn_area,
                            &cn_mask,
                            ext->sn_style,
                            ext->sn_style->text.opa,
                            buffer,
                            LV_TXT_FLAG_EXPAND | LV_TXT_FLAG_CENTER,
                            &offset,
                            NULL,
                            NULL,
                            LV_BIDI_DIR_LTR);
                } else {
                    lv_draw_label(&cn_area,
                            &cn_mask,
                            ext->sn_style,
                            ext->sn_style->text.opa,
                            buffer,
                            LV_TXT_FLAG_EXPAND | LV_TXT_FLAG_CENTER,
                            &offset,
                            NULL,
                            NULL,
                            LV_BIDI_DIR_LTR);
                }
            } else {
                lv_draw_label(&cn_area,
                        &cn_mask,
                        ext->rn_style,
                        ext->rn_style->text.opa,
                        buffer,
                        LV_TXT_FLAG_EXPAND | LV_TXT_FLAG_CENTER,
                        &offset,
                        NULL,
                        NULL,
                        LV_BIDI_DIR_LTR);
            }
        }
    }

    if (ext->metro_note) {
        lv_point_t top = {
            .x = x + ((w * ext->metro_note->start) / YSW_CS_DURATION),
            .y = y,
        };
        lv_point_t bottom = {
            .x = x + ((w * ext->metro_note->start) / YSW_CS_DURATION),
            .y = y + h,
        };
        lv_draw_line(&top, &bottom, mask, ext->mn_style, ext->mn_style->body.border.opa);
    }
}

static bool design_cb(lv_obj_t *cse, const lv_area_t *mask, lv_design_mode_t mode)
{
    bool result = true;
    switch (mode) {
        case LV_DESIGN_COVER_CHK:
            result = super_design_cb(cse, mask, mode);
            break;
        case LV_DESIGN_DRAW_MAIN:
            draw_main(cse, mask, mode);
            break;
        case LV_DESIGN_DRAW_POST:
            break;
    }
    return result;
}

static void fire_select(lv_obj_t *cse, ysw_cn_t *cn)
{
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->event_cb) {
        ysw_lv_cse_event_cb_data_t data = {
            .select.cn = cn,
        };
        ext->event_cb(cse, YSW_LV_CSE_SELECT, &data);
    }
}

static void fire_deselect(lv_obj_t *cse, ysw_cn_t *cn)
{
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->event_cb) {
        ysw_lv_cse_event_cb_data_t data = {
            .deselect.cn = cn,
        };
        ext->event_cb(cse, YSW_LV_CSE_DESELECT, &data);
    }
}

static void fire_create(lv_obj_t *cse, lv_point_t *point)
{
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->event_cb) {
        lv_coord_t w = lv_obj_get_width(cse);
        lv_coord_t h = lv_obj_get_height(cse);

        lv_coord_t x_offset = point->x - cse->coords.x1;
        lv_coord_t y_offset = point->y - cse->coords.y1;

        double pixels_per_tick = (double)w / YSW_CS_DURATION;
        double pixels_per_degree = (double)h / ROW_COUNT;

        lv_coord_t tick_index = x_offset / pixels_per_tick;
        lv_coord_t row_index = y_offset / pixels_per_degree;

        ysw_lv_cse_event_cb_data_t data = {
            .create.start = tick_index,
            .create.degree = YSW_MIDI_UNPO - row_index,
        };

        ext->event_cb(cse, YSW_LV_CSE_CREATE, &data);
    }
}

static void fire_drag_end(lv_obj_t *cse)
{
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->event_cb) {
        ext->event_cb(cse, YSW_LV_CSE_DRAG_END, NULL);
    }
}

static void capture_selection(lv_obj_t *cse, lv_point_t *point)
{
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    ext->dragging = false;
    ext->selected_cn = NULL;
    ext->selection_type = YSW_BOUNDS_NONE;
    ext->last_click = *point;
    uint32_t cn_count = ysw_cs_get_cn_count(ext->cs);
    for (uint8_t i = 0; i < cn_count; i++) {
        lv_area_t cn_area;
        ysw_cn_t *cn = ysw_cs_get_cn(ext->cs, i);
        get_cn_info(cse, cn, &cn_area, NULL, NULL);
        ysw_bounds_t bounds_type = ysw_bounds_check(&cn_area, point);
        if (bounds_type) {
            ext->selected_cn = cn;
            ext->selection_type = bounds_type;
            if (ysw_cn_is_selected(cn)) {
                // return on first selected cn, otherwise pick another one
                return;
            }
        }
    }
}

static void select_only(lv_obj_t *cse, ysw_cn_t *selected_cn)
{
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    uint32_t cn_count = ysw_cs_get_cn_count(ext->cs);
    for (int i = 0; i < cn_count; i++) {
        ysw_cn_t *cn = ysw_cs_get_cn(ext->cs, i);
        if (selected_cn == cn) {
            ysw_cn_select(selected_cn, true);
            fire_select(cse, selected_cn);
        } else {
            if (ysw_cn_is_selected(cn)) {
                ysw_cn_select(cn, false);
                fire_deselect(cse, cn);
            }
        }
    }
}

static void drag_horizontally(lv_obj_t *cse, ysw_cn_t *cn, ysw_cn_t *drag_start_cn, lv_coord_t x)
{
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);

    lv_coord_t w = lv_obj_get_width(cse);
    double ticks_per_pixel = (double)YSW_CS_DURATION / w;
    lv_coord_t delta_ticks = x * ticks_per_pixel;

    //ESP_LOGD(TAG, "drag_horizontally w=%d", w);
    //ESP_LOGD(TAG, "drag_horizontally drag.x=%d", x);
    //ESP_LOGD(TAG, "drag_horizontally ticks_per_pixel=%g", ticks_per_pixel);
    //ESP_LOGD(TAG, "drag_horizontally delta_ticks=%d", delta_ticks);

    bool left_drag = delta_ticks < 0;
    uint32_t ticks = abs(delta_ticks);

    int32_t old_start = drag_start_cn->start;
    int32_t old_duration = drag_start_cn->duration;
    int32_t new_start = old_start;
    int32_t new_duration = old_duration;

    if (ext->selection_type == YSW_BOUNDS_LEFT) {
        if (left_drag) {
            new_duration = old_duration + ticks;
            new_start = old_start - ticks;
        } else {
            new_duration = old_duration - ticks;
            if (new_duration < MINIMUM_DURATION) {
                new_duration = MINIMUM_DURATION;
            }
            new_start = old_start + ticks;
        }
    } else if (ext->selection_type == YSW_BOUNDS_RIGHT) {
        if (left_drag) {
            new_duration = old_duration - ticks;
            if (new_duration < MINIMUM_DURATION) {
                new_duration = MINIMUM_DURATION;
                new_start = old_start - (ticks - old_duration) - new_duration;
            }
        } else {
            new_duration = old_duration + ticks;
        }
    } else {
        if (left_drag) {
            new_start = old_start - ticks;
        } else {
            new_start = old_start + ticks;
        }
    }

    if (new_duration > YSW_CS_DURATION) {
        new_duration = YSW_CS_DURATION;
    }
    if (new_start < 0) {
        new_start = 0;
    }
    if (new_start + new_duration > YSW_CS_DURATION) {
        new_start = YSW_CS_DURATION - new_duration;
    }

    cn->start = new_start;
    cn->duration = new_duration;
}

static void drag_vertically(lv_obj_t *cse, ysw_cn_t *cn, ysw_cn_t *drag_start_cn, lv_coord_t y)
{
    lv_coord_t h = lv_obj_get_height(cse);
    double pixels_per_half_degree = ((double)h / ROW_COUNT) / 2;
    lv_coord_t delta_half_degrees = round(y / pixels_per_half_degree);

    //ESP_LOGD(TAG, "drag_vertically h=%d", h);
    //ESP_LOGD(TAG, "drag_vertically drag.y=%d", y);
    //ESP_LOGD(TAG, "drag_vertically pixels_per_half_degree=%g", pixels_per_half_degree);
    //ESP_LOGD(TAG, "drag_vertically delta_half_degrees=%d", delta_half_degrees);

    bool top_drag = delta_half_degrees < 0;
    uint8_t half_degrees = abs(delta_half_degrees);
    uint8_t degrees = half_degrees / 2;
    bool is_accidental = half_degrees % 2;

    if (top_drag) {
        cn->degree = drag_start_cn->degree + degrees;
        if (is_accidental) {
            if (ysw_cn_is_flat(drag_start_cn)) {
                ysw_cn_set_natural(cn);
            } else if (ysw_cn_is_natural(drag_start_cn)) {
                ysw_cn_set_sharp(cn);
            } else {
                ysw_cn_set_natural(cn);
                cn->degree++;
            }
        } else {
            ysw_cn_set_natural(cn);
        }
        if (cn->degree > YSW_CSN_MAX_DEGREE) {
            cn->degree = YSW_CSN_MAX_DEGREE;
        }
    } else {
        cn->degree = drag_start_cn->degree - degrees;
        if (is_accidental) {
            if (ysw_cn_is_sharp(drag_start_cn)) {
                ysw_cn_set_natural(cn);
            } else if (ysw_cn_is_natural(drag_start_cn)) {
                ysw_cn_set_flat(cn);
            } else {
                ysw_cn_set_natural(cn);
                cn->degree--;
            }
        } else {
            ysw_cn_set_natural(cn);
        }
        if (cn->degree < YSW_CSN_MIN_DEGREE) {
            cn->degree = YSW_CSN_MIN_DEGREE;
        }
    }
}

static void do_drag(lv_obj_t *cse, lv_point_t *point)
{
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);

    lv_coord_t x = point->x - ext->last_click.x;
    lv_coord_t y = point->y - ext->last_click.y;

    bool drag_x = abs(x) > MINIMUM_DRAG;
    bool drag_y = abs(y) > MINIMUM_DRAG;

    ext->dragging = ext->selected_cn && (drag_x || drag_y);

    if (ext->dragging) {

        if (!ysw_cn_is_selected(ext->selected_cn)) {
            select_only(cse, ext->selected_cn);
        }

        if (!ext->drag_start_cs) {
            ESP_LOGE(TAG, "do_drag starting new drag");
            ext->drag_start_cs = ysw_cs_copy(ext->cs);
        }

        ESP_LOGE(TAG, "do_drag dragged x=%d, y=%d from start x=%d, y=%d", x, y, ext->last_click.x, ext->last_click.y);

        uint32_t cn_count = ysw_cs_get_cn_count(ext->cs);
        uint32_t drag_start_cn_count = ysw_cs_get_cn_count(ext->drag_start_cs);
        if (cn_count != drag_start_cn_count) {
            ESP_LOGE(TAG, "expected cn_count=%d to equal drag_start_cn_count=%d", cn_count, drag_start_cn_count);
        } else {
            for (int i = 0; i < cn_count; i++) {
                ysw_cn_t *cn = ysw_cs_get_cn(ext->cs, i);
                ysw_cn_t *drag_start_cn = ysw_cs_get_cn(ext->drag_start_cs, i);
                if (ysw_cn_is_selected(cn)) {
                    if (drag_x) {
                        drag_horizontally(cse, cn, drag_start_cn, x);
                    }
                    if (drag_y) {
                        drag_vertically(cse, cn, drag_start_cn, y);
                    }
                }
            }
        }
    }
}

static void do_click(lv_obj_t *cse)
{
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->long_press) {
        ext->long_press = false;
    } else {
        select_only(cse, ext->selected_cn);
    }
}

static void on_signal_pressed(lv_obj_t *cse, void *param)
{
    lv_indev_t *indev_act = (lv_indev_t*)param;
    lv_indev_proc_t *proc = &indev_act->proc;
    lv_point_t *point = &proc->types.pointer.act_point;
    capture_selection(cse, point);
}

static void on_signal_pressing(lv_obj_t *cse, void *param)
{
    lv_indev_t *indev_act = (lv_indev_t*)param;
    lv_indev_proc_t *proc = &indev_act->proc;
    do_drag(cse, &proc->types.pointer.act_point);
    lv_obj_invalidate(cse);
}

static void on_signal_released(lv_obj_t *cse, void *param)
{
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    lv_indev_t *indev_act = (lv_indev_t*)param;
    lv_indev_proc_t *proc = &indev_act->proc;
    do_drag(cse, &proc->types.pointer.act_point);
    if (ext->dragging) {
        fire_drag_end(cse);
        ext->dragging = false;
        if (ext->drag_start_cs) {
            ysw_cs_free(ext->drag_start_cs);
            ext->drag_start_cs = NULL;
        }
    } else {
        do_click(cse);
    }
    ext->selected_cn = NULL;
    ext->selection_type = YSW_BOUNDS_NONE;
    lv_obj_invalidate(cse);
}

static void on_signal_press_lost(lv_obj_t *cse, void *param)
{
    ESP_LOGD(TAG, "on_signal_press_lost entered");
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->dragging) {
        ext->dragging = false;
        if (ext->drag_start_cs) {
            uint32_t cn_count = ysw_cs_get_cn_count(ext->cs);
            for (uint32_t i = 0; i < cn_count; i++) {
                ysw_cn_t *cn = ysw_cs_get_cn(ext->cs, i);
                ysw_cn_t *drag_start_cn = ysw_cs_get_cn(ext->drag_start_cs, i);
                *cn = *drag_start_cn;
            }
            ysw_cs_free(ext->drag_start_cs);
            ext->drag_start_cs = NULL;
        }
    }
    ext->selected_cn = NULL;
    ext->selection_type = YSW_BOUNDS_NONE;
    lv_obj_invalidate(cse);
}

static void on_signal_long_press(lv_obj_t *cse, void *param)
{
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    lv_indev_t *indev_act = (lv_indev_t*)param;
    lv_indev_proc_t *proc = &indev_act->proc;
    lv_point_t *point = &proc->types.pointer.act_point;
    do_drag(cse, point);
    if (!ext->dragging) {
        if (ext->selected_cn) {
            bool selected = !ysw_cn_is_selected(ext->selected_cn);
            ysw_cn_select(ext->selected_cn, selected);
            if (selected) {
                fire_select(cse, ext->selected_cn);
            } else {
                fire_deselect(cse, ext->selected_cn);
            }
        } else {
            fire_create(cse, point);
        }
        ext->long_press = true;
    }
}

static lv_res_t signal_cb(lv_obj_t *cse, lv_signal_t signal, void *param)
{
    lv_res_t res = super_signal_cb(cse, signal, param);
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
                on_signal_pressed(cse, param);
                break;
            case LV_SIGNAL_PRESSING:
                on_signal_pressing(cse, param);
                break;
            case LV_SIGNAL_RELEASED:
                on_signal_released(cse, param);
                break;
            case LV_SIGNAL_PRESS_LOST:
                on_signal_press_lost(cse, param);
                break;
            case LV_SIGNAL_LONG_PRESS:
                on_signal_long_press(cse, param);
                break;
        }
    }
    return res;
}

lv_obj_t *ysw_lv_cse_create(lv_obj_t *par)
{
    lv_obj_t *cse = lv_obj_create(par, NULL);
    LV_ASSERT_MEM(cse);
    if (cse == NULL) {
        return NULL;
    }

    if (super_signal_cb == NULL) {
        super_signal_cb = lv_obj_get_signal_cb(cse);
    }

    if (super_design_cb == NULL) {
        super_design_cb = lv_obj_get_design_cb(cse);
    }

    ysw_lv_cse_ext_t *ext = lv_obj_allocate_ext_attr(cse, sizeof(ysw_lv_cse_ext_t));
    LV_ASSERT_MEM(ext);
    if (ext == NULL) {
        return NULL;
    }

    ext->cs = NULL;
    ext->last_click.x = 0;
    ext->last_click.y = 0;
    ext->dragging = false;
    ext->long_press = false;
    ext->drag_start_cs = NULL;
    ext->metro_note = NULL;
    ext->bg_style = &lv_style_plain;
    ext->oi_style = &ysw_style_oi;
    ext->ei_style = &ysw_style_ei;
    ext->rn_style = &ysw_style_rn;
    ext->sn_style = &ysw_style_sn;
    ext->mn_style = &ysw_style_mn;
    ext->event_cb = NULL;

    lv_obj_set_signal_cb(cse, signal_cb);
    lv_obj_set_design_cb(cse, design_cb);
    lv_obj_set_click(cse, true);
    lv_obj_set_size(cse, LV_DPI * 2, LV_DPI / 3);
    lv_obj_set_style(cse, ext->bg_style);

    return cse;
}

void ysw_lv_cse_set_cs(lv_obj_t *cse, ysw_cs_t *cs)
{
    LV_ASSERT_OBJ(cse, LV_OBJX_NAME);
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->cs == cs) {
        return;
    }
    ext->cs = cs;
    lv_obj_invalidate(cse);
}

void ysw_lv_cse_set_event_cb(lv_obj_t *cse, ysw_lv_cse_event_cb_t event_cb)
{
    LV_ASSERT_OBJ(cse, LV_OBJX_NAME);
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    ext->event_cb = event_cb;
}

void ysw_lv_cse_on_metro(lv_obj_t *cse, note_t *metro_note)
{
    ysw_lv_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (metro_note != ext->metro_note) {
        ext->metro_note = metro_note;
        lv_obj_invalidate(cse);
    }
}

