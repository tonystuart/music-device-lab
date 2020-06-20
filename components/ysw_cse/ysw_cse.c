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
#include "ysw_sn.h"
#include "ysw_styles.h"
#include "ysw_ticks.h"

#include "lvgl.h"
#include "lv_debug.h"

#include "esp_log.h"

#include "math.h"
#include "stdio.h"

#define TAG "YSW_CSE"
#define LV_OBJX_NAME "ysw_cse"
#define MINIMUM_DRAG 5
#define MINIMUM_DURATION 10
#define ROW_COUNT YSW_MIDI_UNPO

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

static const char *key_labels[] =
{
    "7th",
    NULL,
    "5th",
    NULL,
    "3rd",
    NULL,
    "1st",
};

static void get_metrics(lv_obj_t *cse, metrics_t *m)
{
    m->cse_left = cse->coords.x1;
    m->cse_top = cse->coords.y1;

    m->cse_height = lv_obj_get_height(cse);
    m->cse_width = lv_obj_get_width(cse);
}

static void get_sn_info(lv_obj_t *cse, ysw_sn_t *sn, lv_area_t *ret_area, uint8_t *ret_degree, int8_t *ret_octave)
{
    uint8_t degree;
    int8_t octave;

    metrics_t m;
    get_metrics(cse, &m);

    ysw_degree_normalize(sn->degree, &degree, &octave);
    lv_coord_t delta_y = -ysw_sn_get_accidental(sn) * ((m.cse_height / ROW_COUNT) / 2);

    lv_coord_t left = m.cse_left + (sn->start * m.cse_width) / YSW_CS_DURATION;
    lv_coord_t right = m.cse_left + ((sn->start + sn->duration) * m.cse_width) / YSW_CS_DURATION;
    lv_coord_t top = m.cse_top + (((YSW_MIDI_UNPO - degree) * m.cse_height) / ROW_COUNT) + delta_y;
    lv_coord_t bottom = m.cse_top + ((((YSW_MIDI_UNPO - degree) + 1) * m.cse_height) / ROW_COUNT) + delta_y;

    lv_area_t area = {
        .x1 = left,
        .y1 = top + 4,
        .x2 = right,
        .y2 = bottom - 4
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
    base_design_cb(cse, mask, mode);

    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);

    if (!ext->cs) {
        return;
    }

    metrics_t m;
    get_metrics(cse, &m);

    for (lv_coord_t i = 0; i < ROW_COUNT; i++) {

        lv_area_t row_area = {
            .x1 = m.cse_left,
            .y1 = m.cse_top + ((i * m.cse_height) / ROW_COUNT),
            .x2 = m.cse_left + m.cse_width,
            .y2 = m.cse_top + (((i + 1) * m.cse_height) / ROW_COUNT)
        };

        lv_area_t row_mask;
        if (_lv_area_intersect(&row_mask, mask, &row_area)) {

            if (i & 0x01) {
                lv_draw_rect(&row_area, &row_mask, &rect_dsc);
            } else {
                lv_draw_rect(&row_area, &row_mask, &rect_dsc);

                lv_area_t label_mask;
                lv_area_t label_area = {
                    .x1 = row_area.x1,
                    .y1 = row_area.y1,
                    .x2 = row_area.x1 + 50,
                    .y2 = row_area.y1 + 15
                };
                if (_lv_area_intersect(&label_mask, mask, &label_area)) {
                    lv_draw_label(&label_area, &label_mask, &label_dsc, key_labels[i], NULL);
                }
            }

            if (i) {
                lv_point_t point1 = {
                    .x = row_area.x1,
                    .y = row_area.y1
                };
                lv_point_t point2 = {
                    .x = row_area.x1 + m.cse_width,
                    .y = row_area.y1
                };
                if (i & 0x01) {
                    lv_draw_line(&point1, &point2, mask, &line_dsc);
                } else {
                    lv_draw_line(&point1, &point2, mask, &line_dsc);
                }
            }
        }
    }

    for (int i = 0; i < ext->cs->divisions; i++) {
        lv_point_t point1 = {
            .x = m.cse_left + ((i * m.cse_width) / ext->cs->divisions),
            .y = m.cse_top
        };
        lv_point_t point2 = {
            .x = m.cse_left + ((i * m.cse_width) / ext->cs->divisions),
            .y = m.cse_top + m.cse_height
        };
        lv_draw_line(&point1, &point2, mask, &line_dsc);
    }

    uint32_t sn_count = ysw_cs_get_sn_count(ext->cs);

    for (int i = 0; i < sn_count; i++) {

        ysw_sn_t *sn = ysw_cs_get_sn(ext->cs, i);

        int8_t octave = 0;
        lv_area_t sn_area = {};
        get_sn_info(cse, sn, &sn_area, NULL, &octave);

        lv_area_t sn_mask;

        if (_lv_area_intersect(&sn_mask, mask, &sn_area)) {

            if (ysw_sn_is_selected(sn)) {
                // TODO: remove block if we don't use a separate style for dragging
                if (ext->dragging) {
                    lv_draw_rect(&sn_area, &sn_mask, &rect_dsc);
                } else {
                    lv_draw_rect(&sn_area, &sn_mask, &rect_dsc);
                }
            } else {
                lv_draw_rect(&sn_area, &sn_mask, &rect_dsc);
            }

            char buffer[32];
            if (octave) {
                snprintf(buffer, sizeof(buffer), "%d/%+d", sn->velocity, octave);
            } else {
                snprintf(buffer, sizeof(buffer), "%d", sn->velocity);
            }

            // vertically center the text
            lv_point_t offset = {
                .x = 0,
                .y = ((sn_area.y2 - sn_area.y1) - 20) / 2, //v7.1: ext->rn_style->text.font->line_height) / 2
            };

            if (ysw_sn_is_selected(sn)) {
                // TODO: remove block if we don't use a separate style for dragging
                if (ext->dragging) {
                    lv_draw_label(&sn_area, &sn_mask, &label_dsc, buffer, NULL);
                } else {
                    lv_draw_label(&sn_area, &sn_mask, &label_dsc, buffer, NULL);
                }
            } else {
                lv_draw_label(&sn_area, &sn_mask, &label_dsc, buffer, NULL);
            }
        }
    }

    if (ext->metro_marker != -1) {
        lv_point_t top = {
            .x = m.cse_left + ((m.cse_width * ext->metro_marker) / YSW_CS_DURATION),
            .y = m.cse_top,
        };
        lv_point_t bottom = {
            .x = m.cse_left + ((m.cse_width * ext->metro_marker) / YSW_CS_DURATION),
            .y = m.cse_top + m.cse_height,
        };
        lv_draw_line(&top, &bottom, mask, &line_dsc);
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
            draw_main(cse, mask, mode);
            break;
        case LV_DESIGN_DRAW_POST:
            break;
    }
    return result;
}

static void fire_create(lv_obj_t *cse, uint32_t start, int8_t degree)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->create_cb) {
        ext->create_cb(ext->context, start, degree);
    }
}

static void fire_edit(lv_obj_t *cse, ysw_sn_t *sn)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    if (ext->edit_cb) {
        ext->edit_cb(ext->context, sn);
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
    fire_create(cse, tick_index, YSW_MIDI_UNPO - row_index);
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
            if (new_duration < MINIMUM_DURATION) {
                new_duration = MINIMUM_DURATION;
            }
            new_start = old_start + ticks;
        }
    } else if (ext->click_type == YSW_BOUNDS_RIGHT) {
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

    sn->start = new_start;
    sn->duration = new_duration;
}

static void drag_vertically(lv_obj_t *cse, ysw_sn_t *sn, ysw_sn_t *drag_start_sn, lv_coord_t y)
{
    metrics_t m;
    get_metrics(cse, &m);
    double pixels_per_half_degree = ((double)m.cse_height / ROW_COUNT) / 2;
    lv_coord_t delta_half_degrees = round(y / pixels_per_half_degree);

    //ESP_LOGD(TAG, "drag_vertically m.cse_height=%d", m.cse_height);
    //ESP_LOGD(TAG, "drag_vertically drag.y=%d", y);
    //ESP_LOGD(TAG, "drag_vertically pixels_per_half_degree=%g", pixels_per_half_degree);
    //ESP_LOGD(TAG, "drag_vertically delta_half_degrees=%d", delta_half_degrees);

    bool top_drag = delta_half_degrees < 0;
    uint8_t half_degrees = abs(delta_half_degrees);
    uint8_t degrees = half_degrees / 2;
    bool is_accidental = half_degrees % 2;

    if (top_drag) {
        sn->degree = drag_start_sn->degree + degrees;
        if (is_accidental) {
            if (ysw_sn_is_flat(drag_start_sn)) {
                ysw_sn_set_natural(sn);
            } else if (ysw_sn_is_natural(drag_start_sn)) {
                ysw_sn_set_sharp(sn);
            } else {
                ysw_sn_set_natural(sn);
                sn->degree++;
            }
        } else {
            ysw_sn_set_natural(sn);
        }
        if (sn->degree > YSW_CSN_MAX_DEGREE) {
            sn->degree = YSW_CSN_MAX_DEGREE;
        }
    } else {
        sn->degree = drag_start_sn->degree - degrees;
        if (is_accidental) {
            if (ysw_sn_is_sharp(drag_start_sn)) {
                ysw_sn_set_natural(sn);
            } else if (ysw_sn_is_natural(drag_start_sn)) {
                ysw_sn_set_flat(sn);
            } else {
                ysw_sn_set_natural(sn);
                sn->degree--;
            }
        } else {
            ysw_sn_set_natural(sn);
        }
        if (sn->degree < YSW_CSN_MIN_DEGREE) {
            sn->degree = YSW_CSN_MIN_DEGREE;
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
        get_sn_info(cse, sn, &sn_area, NULL, NULL);
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
        lv_indev_t *indev_act = (lv_indev_t*)param;
        lv_indev_wait_release(indev_act);
        if (ext->clicked_sn) {
            fire_edit(cse, ext->clicked_sn);
        } else {
            lv_indev_proc_t *proc = &indev_act->proc;
            lv_point_t *point = &proc->types.pointer.act_point;
            prepare_create(cse, point);
        }
        ext->long_press = true;
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

lv_obj_t *ysw_cse_create(lv_obj_t *par, void *context)
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

    *ext = (ysw_cse_ext_t){
        .metro_marker = -1,
        //v7: .bg_style = &lv_style_plain,
        //v7: .oi_style = &ysw_style_oi,
        //v7: .ei_style = &ysw_style_ei,
        //v7: .rn_style = &ysw_style_rn,
        //v7: .sn_style = &ysw_style_sn,
        //v7: .mn_style = &ysw_style_mn,
        .context = context,
    };

    //v7: lv_obj_set_style(cse, ext->bg_style);

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

void ysw_cse_set_edit_cb(lv_obj_t *cse, void *cb)
{
    ysw_cse_ext_t *ext = lv_obj_get_ext_attr(cse);
    ext->edit_cb = cb;
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

