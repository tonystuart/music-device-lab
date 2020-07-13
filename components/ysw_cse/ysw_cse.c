// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// Chord Style Editor (CSE)

#include "ysw_cse.h"

#include "ysw_cs.h"
#include "ysw_degree.h"
#include "ysw_quatone.h"
#include "ysw_sn.h"
#include "ysw_style.h"
#include "ysw_ticks.h"

#include "lvgl.h"
#include "lv_debug.h"

#include "esp_log.h"

#include "math.h"
#include "stdio.h"
#include "stdlib.h"

#define TAG "YSW_CSE"
#define LV_OBJX_NAME "ysw_cse"
#define MINIMUM_DRAG 5
#define ROW_COUNT YSW_QUATONES_PER_OCTAVE

ysw_cse_gs_t ysw_cse_gs = {
    .auto_play_all = YSW_AUTO_PLAY_OFF,
    .auto_play_last = YSW_AUTO_PLAY_PLAY,
    .multiple_selection = false,
};

typedef struct {
    lv_coord_t cse_left;
    lv_coord_t cse_top;
    lv_coord_t cse_height;
    lv_coord_t cse_width;
} metrics_t;

static lv_design_cb_t base_design_cb;
static lv_signal_cb_t base_signal_cb;

static void get_metrics(lv_obj_t *cse, metrics_t *m)
{
    m->cse_left = cse->coords.x1;
    m->cse_top = cse->coords.y1;

    m->cse_height = lv_obj_get_height(cse);
    m->cse_width = lv_obj_get_width(cse);
}

static void get_sn_info(lv_obj_t *cse, ysw_sn_t *sn, lv_area_t *ret_area)
{
    metrics_t m;
    get_metrics(cse, &m);

    uint8_t offset = ysw_quatone_get_offset(sn->quatone);
    int row = to_index(YSW_QUATONES_PER_OCTAVE) - offset;

    int top_row = row - 1; // use two rows to display the note
    int bottom_row = row + 1;

    lv_coord_t left = m.cse_left + ysw_common_muldiv(sn->start, m.cse_width, YSW_CS_DURATION);
    lv_coord_t right = m.cse_left + ysw_common_muldiv(sn->start + sn->duration, m.cse_width, YSW_CS_DURATION);
    lv_coord_t top = m.cse_top + ysw_common_muldiv(top_row, m.cse_height, ROW_COUNT);
    lv_coord_t bottom = m.cse_top + ysw_common_muldiv(bottom_row, m.cse_height, ROW_COUNT);

    lv_area_t area = {
        .x1 = left,
        .y1 = top + 4,
        .x2 = right,
        .y2 = bottom - 4
    };

    if (ret_area) {
        *ret_area = area;
    }

}

static void get_style_note_label(ysw_sn_t *sn, char *label, uint32_t size)
{
    int8_t octave = ysw_quatone_get_octave(sn->quatone);
    int8_t offset = ysw_quatone_get_offset(sn->quatone);
    const char *quatone_name = ysw_quatone_offset_cardinal[offset];

    if (octave) {
        snprintf(label, size, "%s%+d", quatone_name, octave);
    } else {
        snprintf(label, size, "%s", quatone_name);
    }
}

static void draw_background(metrics_t *m, const lv_area_t *mask)
{
    bool use_even_style = true;
    for (lv_coord_t i = 0; i < ROW_COUNT; i += 2) {

        lv_coord_t top_row = i; // use two rows for each division
        lv_coord_t bottom_row = i + 2;

        lv_area_t row_area = {
            .x1 = m->cse_left,
            .x2 = m->cse_left + m->cse_width,
            .y1 = m->cse_top + ysw_common_muldiv(top_row, m->cse_height, ROW_COUNT),
            .y2 = m->cse_top + ysw_common_muldiv(bottom_row, m->cse_height, ROW_COUNT),
        };

        lv_area_t row_mask;
        if (_lv_area_intersect(&row_mask, mask, &row_area)) {

            if (use_even_style) {
                lv_draw_rect(&row_area, &row_mask, &even_rect_dsc);
            } else {
                lv_draw_rect(&row_area, &row_mask, &odd_rect_dsc);
            }

            if (i) {
                lv_point_t point1 = {
                    .x = row_area.x1,
                    .y = row_area.y1
                };
                lv_point_t point2 = {
                    .x = row_area.x1 + m->cse_width,
                    .y = row_area.y1
                };
                if (use_even_style) {
                    lv_draw_line(&point1, &point2, mask, &even_line_dsc);
                } else {
                    lv_draw_line(&point1, &point2, mask, &odd_line_dsc);
                }
            }
        }
        use_even_style = !use_even_style;
    }
}

static void draw_divisions(uint32_t divisions, metrics_t *m, const lv_area_t *mask)
{
    for (int i = 1; i < divisions; i++) {
        lv_point_t point1 = {
            .x = m->cse_left + ysw_common_muldiv(i, m->cse_width, divisions),
            .y = m->cse_top
        };
        lv_point_t point2 = {
            .x = m->cse_left + ysw_common_muldiv(i, m->cse_width, divisions),
            .y = m->cse_top + m->cse_height
        };
        lv_draw_line(&point1, &point2, mask, &div_line_dsc);
    }
}

static void draw_sn(lv_obj_t *cse, const lv_area_t *mask)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    uint32_t sn_count = ysw_cs_get_sn_count(ext->cs);

    for (int i = 0; i < sn_count; i++) {

        ysw_sn_t *sn = ysw_cs_get_sn(ext->cs, i);

        lv_area_t sn_area = { };
        get_sn_info(cse, sn, &sn_area);

        lv_area_t sn_mask;

        if (_lv_area_intersect(&sn_mask, mask, &sn_area)) {

            char buffer[32];
            get_style_note_label(sn, buffer, sizeof(buffer));
            if (ysw_sn_is_selected(sn)) {
                if (ext->dragging) {
                    lv_draw_rect(&sn_area, &sn_mask, &drag_sn_rect_dsc);
                    lv_draw_label(&sn_area, &sn_mask, &drag_sn_label_dsc, buffer, NULL);
                } else {
                    lv_draw_rect(&sn_area, &sn_mask, &sel_sn_rect_dsc);
                    lv_draw_label(&sn_area, &sn_mask, &sel_sn_label_dsc, buffer, NULL);
                }
            } else {
                lv_draw_rect(&sn_area, &sn_mask, &sn_rect_dsc);
                lv_draw_label(&sn_area, &sn_mask, &sn_label_dsc, buffer, NULL);
            }
        }
    }
}

static void draw_metro_marker(int32_t metro_marker, metrics_t *m, const lv_area_t *mask)
{
    if (metro_marker != -1) {
        lv_point_t top = {
            .x = m->cse_left + ysw_common_muldiv(m->cse_width, metro_marker, YSW_CS_DURATION),
            .y = m->cse_top,
        };
        lv_point_t bottom = {
            .x = m->cse_left + ysw_common_muldiv(m->cse_width, metro_marker, YSW_CS_DURATION),
            .y = m->cse_top + m->cse_height,
        };
        lv_draw_line(&top, &bottom, mask, &metro_line_dsc);
    }
}

static void draw_main(lv_obj_t *cse, const lv_area_t *mask)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->cs) {
        metrics_t m;
        get_metrics(cse, &m);
        draw_background(&m, mask);
        draw_divisions(ext->cs->divisions, &m, mask);
        draw_sn(cse, mask);
        draw_metro_marker(ext->metro_marker, &m, mask);
    }
}

static lv_design_res_t design_cb(lv_obj_t *cse, const lv_area_t *mask, lv_design_mode_t mode)
{
    bool result = true;
    switch (mode) {
        case LV_DESIGN_COVER_CHK:
            result = base_design_cb(cse, mask, mode);
            break;
        case LV_DESIGN_DRAW_MAIN:
            base_design_cb(cse, mask, mode);
            draw_main(cse, mask);
            break;
        case LV_DESIGN_DRAW_POST:
            break;
    }
    return result;
}

static void fire_create(lv_obj_t *cse, uint32_t start, ysw_quatone_t quatone)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->create_cb) {
        ext->create_cb(ext->context, start, quatone);
    }
}

static void fire_select(lv_obj_t *cse, ysw_sn_t *sn)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->select_cb) {
        ext->select_cb(ext->context, sn);
    }
}

static void fire_deselect(lv_obj_t *cse, ysw_sn_t *sn)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->deselect_cb) {
        ext->deselect_cb(ext->context, sn);
    }
}

static void fire_drag_end(lv_obj_t *cse)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->drag_end_cb) {
        ext->drag_end_cb(ext->context);
    }
}

static void select_sn(lv_obj_t *cse, ysw_sn_t *sn)
{
    ysw_sn_select(sn, true);
    fire_select(cse, sn);
}

static void deselect_sn(lv_obj_t *cse, ysw_sn_t *sn)
{
    ysw_sn_select(sn, false);
    fire_deselect(cse, sn);
}

static void deselect_all(lv_obj_t *cse)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    uint32_t sn_count = ysw_cs_get_sn_count(ext->cs);
    for (int i = 0; i < sn_count; i++) {
        ysw_sn_t *sn = ysw_cs_get_sn(ext->cs, i);
        if (ysw_sn_is_selected(sn)) {
            deselect_sn(cse, sn);
        }
    }
}

static void prepare_create(lv_obj_t *cse, lv_point_t *point)
{
    metrics_t m;
    get_metrics(cse, &m);
    lv_coord_t x_offset = point->x - cse->coords.x1;
    lv_coord_t y_offset = point->y - cse->coords.y1;
    double pixels_per_tick = (double)m.cse_width / YSW_CS_DURATION;
    double pixels_per_degree = (double)m.cse_height / ROW_COUNT;
    lv_coord_t tick_index = x_offset / pixels_per_tick;
    lv_coord_t row_index = y_offset / pixels_per_degree;
    if (!ysw_cse_gs.multiple_selection) {
        deselect_all(cse);
    }
    ysw_quatone_t quatone = ysw_quatone_create(0, to_index(YSW_QUATONES_PER_OCTAVE) - row_index);
    fire_create(cse, tick_index, quatone);
}

static void drag_horizontally(lv_obj_t *cse, ysw_sn_t *sn, ysw_sn_t *drag_start_sn, lv_coord_t x)
{
    metrics_t m;
    get_metrics(cse, &m);
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);

    double ticks_per_pixel = (double)YSW_CS_DURATION / m.cse_width;
    lv_coord_t delta_ticks = x * ticks_per_pixel;

    //ESP_LOGD(TAG, "drag_horizontally m.cse_width=%d", m.cse_width);
    //ESP_LOGD(TAG, "drag_horizontally drag.x=%d", x);
    //ESP_LOGD(TAG, "drag_horizontally ticks_per_pixel=%g", ticks_per_pixel);
    //ESP_LOGD(TAG, "drag_horizontally delta_ticks=%d", delta_ticks);

    bool left_drag = delta_ticks < 0;
    uint32_t ticks = abs(delta_ticks);

    int32_t old_start = drag_start_sn->start;
    int32_t old_duration = drag_start_sn->duration;
    int32_t new_start = old_start;
    int32_t new_duration = old_duration;

    if (ext->click_type == YSW_BOUNDS_LEFT) {
        if (left_drag) {
            new_duration = old_duration + ticks;
            new_start = old_start - ticks;
        } else {
            new_duration = old_duration - ticks;
            if (new_duration < YSW_CSN_MIN_DURATION) {
                new_duration = YSW_CSN_MIN_DURATION;
            }
            new_start = old_start + ticks;
        }
    } else if (ext->click_type == YSW_BOUNDS_RIGHT) {
        if (left_drag) {
            new_duration = old_duration - ticks;
            if (new_duration < YSW_CSN_MIN_DURATION) {
                new_duration = YSW_CSN_MIN_DURATION;
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

    sn->start = new_start;
    sn->duration = new_duration;
}

// TODO: quatone cleanup
static void drag_vertically(lv_obj_t *cse, ysw_sn_t *sn, ysw_sn_t *drag_start_sn, lv_coord_t y)
{
    metrics_t m;
    get_metrics(cse, &m);
    double pixels_per_quatone = (double)m.cse_height / ROW_COUNT;
    lv_coord_t delta_quatones = round(y / pixels_per_quatone);
    bool top_drag = delta_quatones < 0;
    uint8_t quatones = abs(delta_quatones);

    if (top_drag) {
        sn->quatone = drag_start_sn->quatone + quatones;
        if (sn->quatone > YSW_QUATONE_MAX) {
            sn->quatone = YSW_QUATONE_MAX;
        }
    } else {
        if (quatones > drag_start_sn->quatone) {
            sn->quatone = 0;
        } else {
            sn->quatone = drag_start_sn->quatone - quatones;
        }
    }
}

static void capture_drag(lv_obj_t *cse, lv_coord_t x, lv_coord_t y)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (!ext->dragging && ext->clicked_sn) {
        bool drag_x = abs(x) > MINIMUM_DRAG;
        bool drag_y = abs(y) > MINIMUM_DRAG;
        ext->dragging = drag_x || drag_y;
        if (ext->dragging) {
            if (!ysw_sn_is_selected(ext->clicked_sn)) {
                deselect_all(cse);
                select_sn(cse, ext->clicked_sn);
            }
            ext->drag_start_cs = ysw_cs_copy(ext->cs);
        }
    }
}

static void capture_click(lv_obj_t *cse, lv_point_t *point)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    ext->clicked_sn = NULL;
    ext->click_type = YSW_BOUNDS_NONE;
    ext->click_point = *point;
    uint32_t sn_count = ysw_cs_get_sn_count(ext->cs);
    for (uint8_t i = 0; i < sn_count; i++) {
        lv_area_t sn_area;
        ysw_sn_t *sn = ysw_cs_get_sn(ext->cs, i);
        get_sn_info(cse, sn, &sn_area);
        ysw_bounds_t bounds_type = ysw_bounds_check(&sn_area, point);
        if (bounds_type) {
            ext->clicked_sn = sn;
            ext->click_type = bounds_type;
            if (ysw_sn_is_selected(sn)) {
                // return on first selected sn, otherwise pick last one
                return;
            }
        }
    }
}

static void on_signal_pressed(lv_obj_t *cse, void *param)
{
    lv_indev_t *indev_act = (lv_indev_t*)param;
    lv_indev_proc_t *proc = &indev_act->proc;
    lv_point_t *point = &proc->types.pointer.act_point;
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->press_lost) {
        // user has dragged back into cse widget
        ext->press_lost = false;
    } else {
        // user has clicked on cse widget
        ext->dragging = false;
        ext->long_press = false;
        ext->press_lost = false;
        capture_click(cse, point);
    }
}

static void on_signal_pressing(lv_obj_t *cse, void *param)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    lv_indev_t *indev_act = (lv_indev_t*)param;
    lv_indev_proc_t *proc = &indev_act->proc;
    lv_point_t *point = &proc->types.pointer.act_point;
    lv_coord_t x = point->x - ext->click_point.x;
    lv_coord_t y = point->y - ext->click_point.y;
    capture_drag(cse, x, y);
    if (ext->dragging) {
        uint32_t sn_count = ysw_cs_get_sn_count(ext->cs);
        for (int i = 0; i < sn_count; i++) {
            ysw_sn_t *sn = ysw_cs_get_sn(ext->cs, i);
            ysw_sn_t *drag_start_sn = ysw_cs_get_sn(ext->drag_start_cs, i);
            if (ysw_sn_is_selected(sn)) {
                drag_horizontally(cse, sn, drag_start_sn, x);
                drag_vertically(cse, sn, drag_start_sn, y);
            }
        }
        lv_obj_invalidate(cse);
    }
}

static void on_signal_released(lv_obj_t *cse, void *param)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->dragging) {
        fire_drag_end(cse);
        ysw_cs_free(ext->drag_start_cs);
    } else if (ext->long_press) {
        // reset flag below for all cases
    } else if (ext->press_lost) {
        // reset flag below for all cases
    } else if (ext->clicked_sn) {
        bool selected = ysw_sn_is_selected(ext->clicked_sn);
        if (selected) {
            deselect_sn(cse, ext->clicked_sn);
        } else {
            if (!ysw_cse_gs.multiple_selection) {
                deselect_all(cse);
            }
            select_sn(cse, ext->clicked_sn);
        }
    } else {
        deselect_all(cse);
    }
    ext->dragging = false;
    ext->drag_start_cs = NULL;
    ext->long_press = false;
    ext->press_lost = false;
    ext->clicked_sn = NULL;
    ext->click_type = YSW_BOUNDS_NONE;
    lv_obj_invalidate(cse);
}

static void on_signal_press_lost(lv_obj_t *cse, void *param)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    ext->press_lost = true;
}

static void on_signal_long_press(lv_obj_t *cse, void *param)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (!ext->dragging) {
        if (ext->clicked_sn) {
            // handle like short press
        } else {
            lv_indev_t *indev_act = (lv_indev_t*)param;
            lv_indev_wait_release(indev_act);
            lv_indev_proc_t *proc = &indev_act->proc;
            lv_point_t *point = &proc->types.pointer.act_point;
            prepare_create(cse, point);
            ext->long_press = true;
        }
    }
}

static lv_res_t signal_cb(lv_obj_t *cse, lv_signal_t signal, void *param)
{
    lv_res_t res = base_signal_cb(cse, signal, param);
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

lv_obj_t* ysw_cse_create(lv_obj_t *par, void *context)
{
    lv_obj_t *cse = lv_obj_create(par, NULL);
    LV_ASSERT_MEM(cse);
    if (cse == NULL) {
        return NULL;
    }

    if (base_signal_cb == NULL) {
        base_signal_cb = lv_obj_get_signal_cb(cse);
    }

    if (base_design_cb == NULL) {
        base_design_cb = lv_obj_get_design_cb(cse);
    }

    ysw_cse_ext_t *ext = lv_obj_allocate_ext_attr(cse, sizeof(ysw_cse_ext_t));
    LV_ASSERT_MEM(ext);
    if (ext == NULL) {
        return NULL;
    }

    *ext = (ysw_cse_ext_t ) {
                .metro_marker = -1,
                .context = context,
            };

    lv_obj_set_signal_cb(cse, signal_cb);
    lv_obj_set_design_cb(cse, design_cb);

    lv_obj_set_click(cse, true);
    lv_theme_apply(cse, LV_THEME_OBJ);

    return cse;
}

void ysw_cse_set_cs(lv_obj_t *cse, ysw_cs_t *cs)
{
    LV_ASSERT_OBJ(cse, LV_OBJX_NAME);
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->cs == cs) {
        return;
    }
    ext->cs = cs;
    lv_obj_invalidate(cse);
}

void ysw_cse_set_create_cb(lv_obj_t *cse, void *cb)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    ext->create_cb = cb;
}

void ysw_cse_set_select_cb(lv_obj_t *cse, void *cb)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    ext->select_cb = cb;
}

void ysw_cse_set_deselect_cb(lv_obj_t *cse, void *cb)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    ext->deselect_cb = cb;
}

void ysw_cse_set_drag_end_cb(lv_obj_t *cse, void *cb)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    ext->drag_end_cb = cb;
}

void ysw_cse_on_metro(lv_obj_t *cse, ysw_note_t *metro_note)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    ESP_LOGD(TAG, "metro_note=%p", metro_note);
    int32_t metro_marker = metro_note ? metro_note->start : -1;
    ESP_LOGD(TAG, "metro_marker=%d", metro_marker);
    if (metro_marker != ext->metro_marker) {
        ext->metro_marker = metro_marker;
        lv_obj_invalidate(cse);
    }
}

