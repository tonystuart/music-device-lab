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

#define MINIMUM_DRAG 10

ysw_lv_cpe_gs_t ysw_lv_cpe_gs = {
    .auto_scroll = true,
    .auto_play = false,
};

typedef struct {
    lv_coord_t cpe_left;
    lv_coord_t cpe_top;
    lv_coord_t cpe_height;
    lv_coord_t cpe_width;
    lv_coord_t col_count;
    lv_coord_t col_width;
    lv_coord_t row_height;
    lv_coord_t cp_left;
    lv_coord_t cp_top;
    lv_coord_t cp_width;
    lv_coord_t cp_height;
    lv_coord_t min_scroll_left;
} metrics_t;

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

static void get_metrics(lv_obj_t *cpe, metrics_t *m)
{
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    uint32_t step_count = ysw_cp_get_step_count(ext->cp);

    m->cpe_left = cpe->coords.x1;
    m->cpe_top = cpe->coords.y1;

    m->cpe_height = lv_obj_get_height(cpe);
    m->cpe_width = lv_obj_get_width(cpe);

    m->col_count = step_count > 16 ? 16 : step_count > 0 ? step_count : 1;
    m->col_width = m->cpe_width / m->col_count;
    m->row_height = m->cpe_height / 9; // 9 = header + 7 degrees + footer

    m->cp_left = m->cpe_left + ext->scroll_left;
    m->cp_top = m->cpe_top + m->row_height;

    m->cp_width = step_count * m->col_width;
    m->cp_height = m->cpe_height - (2 * m->row_height);

    m->min_scroll_left = m->cpe_width - m->cp_width;
}

static void draw_main(lv_obj_t *cpe, const lv_area_t *mask, lv_design_mode_t mode)
{
    super_design_cb(cpe, mask, mode);

    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);

    if (ext->metro_note && ysw_lv_cpe_gs.auto_scroll) {
        ysw_lv_cpe_ensure_visible(cpe, ext->metro_note->velocity, ext->metro_note->velocity);
    }

    metrics_t m;
    get_metrics(cpe, &m);

    uint32_t step_count = ysw_cp_get_step_count(ext->cp);

    lv_point_t top_left = {
        .x = m.cpe_left,
        .y = m.cp_top,
    };

    lv_point_t top_right = {
        .x = m.cpe_left + m.cpe_width,
        .y = m.cp_top,
    };

    lv_draw_line(&top_left, &top_right, mask, ext->fg_style, ext->fg_style->body.border.opa);

    lv_point_t bottom_left = {
        .x = m.cpe_left,
        .y = m.cp_top + m.cp_height,
    };

    lv_point_t bottom_right = {
        .x = m.cpe_left + m.cpe_width,
        .y = m.cp_top + m.cp_height,
    };

    lv_draw_line(&bottom_left, &bottom_right, mask, ext->fg_style, ext->fg_style->body.border.opa);

    uint8_t steps_in_measures[step_count];

    ysw_cp_get_steps_in_measures(ext->cp, steps_in_measures, step_count);

    uint32_t measure = 0;

    ysw_step_t *first_selected_step = NULL;

    for (uint32_t i = 0; i < step_count; i++) {
        ysw_step_t *step = ysw_cp_get_step(ext->cp, i);

        lv_coord_t left = m.cp_left + (i * m.col_width);

        if (!i || ysw_step_is_new_measure(step)) {

            lv_area_t heading_area = {
                .x1 = left,
                .y1 = m.cpe_top,
                .x2 = left + (steps_in_measures[measure] * m.col_width),
                .y2 = m.cp_top,
            };

            measure++;
            lv_area_t heading_mask;

            if (lv_area_intersect(&heading_mask, mask, &heading_area)) {
                lv_point_t offset = {
                    .x = 0,
                    .y = ((heading_area.y2 - heading_area.y1) - ext->fg_style->text.font->line_height) / 2,
                };

                char buffer[32];
                ysw_itoa(measure, buffer, sizeof(buffer));

                lv_draw_label(&heading_area,
                        &heading_mask,
                        ext->fg_style,
                        ext->fg_style->text.opa,
                        buffer,
                        LV_TXT_FLAG_EXPAND | LV_TXT_FLAG_CENTER,
                        &offset,
                        NULL,
                        NULL,
                        LV_BIDI_DIR_LTR);
            }

        }

        if (i) {
            lv_coord_t col_top = ysw_step_is_new_measure(step) ? m.cpe_top : m.cp_top;
            lv_point_t top = {
                .x = left,
                .y = col_top,
            };
            lv_point_t bottom = {
                .x = left,
                .y = m.cp_top + m.cp_height,
            };
            lv_draw_line(&top, &bottom, mask, ext->fg_style, ext->fg_style->body.border.opa);
        }

        lv_coord_t cell_top = m.cp_top + ((YSW_MIDI_UNPO - step->degree) * m.row_height);

        lv_area_t cell_area = {
            .x1 = left,
            .y1 = cell_top,
            .x2 = left + m.col_width,
            .y2 = cell_top + m.row_height,
        };

        lv_area_t cell_mask;

        if (lv_area_intersect(&cell_mask, mask, &cell_area)) {

              if (ysw_step_is_selected(step)) {
                  lv_draw_rect(&cell_area, &cell_mask, ext->ss_style, ext->ss_style->body.opa);
              } else {
                  lv_draw_rect(&cell_area, &cell_mask, ext->rs_style, ext->rs_style->body.opa);
              }

        // vertically center the text
        lv_point_t offset = {
            .x = 0,
            .y = ((cell_area.y2 - cell_area.y1) - ext->fg_style->text.font->line_height) / 2,
        };

        lv_draw_label(&cell_area,
                &cell_mask,
                ext->fg_style,
                ext->fg_style->text.opa,
                key_labels[to_index(step->degree)],
                LV_TXT_FLAG_EXPAND | LV_TXT_FLAG_CENTER,
                &offset,
                NULL,
                NULL,
                LV_BIDI_DIR_LTR);
        }

        //if (step == ext->selected_step) {
        if (!first_selected_step && ysw_step_is_selected(step)) {
            first_selected_step = step;
            lv_area_t footer_area = {
                .x1 = m.cpe_left,
                .y1 = m.cp_top + m.cp_height,
                .x2 = m.cpe_left + m.cpe_width,
                .y2 = m.cp_top + m.cp_height + m.row_height,
            };

            lv_area_t footer_mask;

            if (lv_area_intersect(&footer_mask, mask, &footer_area)) {

                // vertically center the text
                lv_point_t offset = {
                    .x = 0,
                    .y = ((footer_area.y2 - footer_area.y1) - ext->fg_style->text.font->line_height) / 2
                };

                lv_draw_label(&footer_area,
                        &footer_mask,
                        ext->fg_style,
                        ext->fg_style->text.opa,
                        step->cs->name,
                        LV_TXT_FLAG_EXPAND | LV_TXT_FLAG_CENTER,
                        &offset,
                        NULL,
                        NULL,
                        LV_BIDI_DIR_LTR);
            }
        }

    }
    if (ext->metro_note) {
        lv_coord_t left = m.cp_left + (ext->metro_note->velocity * m.col_width);
        lv_point_t top = {
            .x = left,
            .y = m.cp_top,
        };
        lv_point_t bottom = {
            .x = left,
            .y = m.cp_top + m.cp_height,
        };
        lv_draw_line(&top, &bottom, mask, ext->ms_style, ext->ms_style->body.border.opa);
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
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    if (ext->event_cb) {
        ysw_lv_cpe_event_cb_data_t data = {
            .select.step = step,
        };
        ext->event_cb(cpe, YSW_LV_CPE_SELECT, &data);
    }
}

static void fire_deselect(lv_obj_t *cpe, ysw_step_t *step)
{
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    if (ext->event_cb) {
        ysw_lv_cpe_event_cb_data_t data = {
            .deselect.step = step,
        };
        ext->event_cb(cpe, YSW_LV_CPE_DESELECT, &data);
    }
}

static void fire_create(lv_obj_t *cpe, uint32_t step_index, uint8_t degree)
{
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    if (ext->event_cb) {
        ysw_lv_cpe_event_cb_data_t data = {
            .create.step_index = step_index,
            .create.degree = degree,
        };
        ext->event_cb(cpe, YSW_LV_CPE_CREATE, &data);
    }
}

static void fire_drag_end(lv_obj_t *cpe)
{
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    if (ext->event_cb) {
        ext->event_cb(cpe, YSW_LV_CPE_DRAG_END, NULL);
    }
}

static void get_step_area(lv_obj_t *cpe, uint32_t step_index, lv_area_t *ret_area)
{
    metrics_t m;
    get_metrics(cpe, &m);

    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    ysw_step_t *step = ysw_cp_get_step(ext->cp, step_index);

    lv_coord_t left = m.cp_left + (step_index * m.col_width);
    lv_coord_t cell_top = m.cp_top + ((YSW_MIDI_UNPO - step->degree) * m.row_height);

    lv_area_t cell_area = {
        .x1 = left,
        .y1 = cell_top,
        .x2 = left + m.col_width,
        .y2 = cell_top + m.row_height,
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
    ext->last_click = *point;
    uint32_t step_count = ysw_cp_get_step_count(ext->cp);
    for (uint8_t i = 0; i < step_count; i++) {
        lv_area_t step_area;
        ysw_step_t *step = ysw_cp_get_step(ext->cp, i);
        get_step_area(cpe, i, &step_area);
        if ((step_area.x1 <= point->x && point->x <= step_area.x2) &&
            (step_area.y1 <= point->y && point->y <= step_area.y2)) {
                // dragging step
                ext->selected_step = step;
                if (ysw_step_is_selected(step)) {
                    // return on first selected step, otherwise pick last one
                    return;
                }
            }
    }
    if (!ext->selected_step) {
        // scrolling screen
        ext->drag_start_scroll_left = ext->scroll_left;
        ESP_LOGD(TAG, "capture drag_start_scroll_left=%d", ext->drag_start_scroll_left);
    }
}

static void select_only(lv_obj_t *cpe, ysw_step_t *selected_step)
{
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    uint32_t step_count = ysw_cp_get_step_count(ext->cp);
    for (int i = 0; i < step_count; i++) {
        ysw_step_t *step = ysw_cp_get_step(ext->cp, i);
        if (selected_step == step) {
            ysw_step_select(selected_step, true);
            fire_select(cpe, selected_step);
        } else {
            if (ysw_step_is_selected(step)) {
                ysw_step_select(step, false);
                fire_deselect(cpe, step);
            }
        }
    }
}

static void drag_vertically(lv_obj_t *cpe, ysw_step_t *step, ysw_step_t *drag_start_step, lv_coord_t y)
{
    metrics_t m;
    get_metrics(cpe, &m);
    uint8_t new_degree = drag_start_step->degree - (y / m.row_height);
    if (new_degree >= 1 && new_degree <= YSW_MIDI_UNPO) {
        step->degree = new_degree;
    }
}

static void scroll_horizontally(lv_obj_t *cpe, lv_coord_t x)
{
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    metrics_t m;
    get_metrics(cpe, &m);
    lv_coord_t new_scroll_left = ext->drag_start_scroll_left + x;
    if (new_scroll_left < m.min_scroll_left) {
        new_scroll_left = m.min_scroll_left;
    }
    if (new_scroll_left > 0) {
        new_scroll_left = 0;
    }
    ext->scroll_left = new_scroll_left;
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

        if (ext->selected_step) {

            if (!ysw_step_is_selected(ext->selected_step)) {
                select_only(cpe, ext->selected_step);
            }

            if (!ext->drag_start_cp) {
                ESP_LOGE(TAG, "do_drag starting new drag");
                ext->drag_start_cp = ysw_cp_copy(ext->cp);
            }

            ESP_LOGE(TAG, "do_drag dragged x=%d, y=%d from start x=%d, y=%d, step=%p", x, y, ext->last_click.x, ext->last_click.y, ext->selected_step);

            uint32_t step_count = ysw_cp_get_step_count(ext->cp);
            uint32_t drag_start_step_count = ysw_cp_get_step_count(ext->drag_start_cp);
            if (step_count != drag_start_step_count) {
                ESP_LOGE(TAG, "expected step_count=%d to equal drag_start_step_count=%d", step_count, drag_start_step_count);
            } else {
                for (int i = 0; i < step_count; i++) {
                    ysw_step_t *step = ysw_cp_get_step(ext->cp, i);
                    ysw_step_t *drag_start_step = ysw_cp_get_step(ext->drag_start_cp, i);
                    if (ysw_step_is_selected(step)) {
                        if (drag_y) {
                            drag_vertically(cpe, step, drag_start_step, y);
                        }
                    }
                }
            }
        } else {
            if (drag_x) {
                scroll_horizontally(cpe, x);
            }
        }
    }
}

static void do_click(lv_obj_t *cpe)
{
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    if (ext->long_press) {
        ext->long_press = false;
    } else {
        select_only(cpe, ext->selected_step);
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
        if (ext->selected_step) {
            fire_drag_end(cpe);
            ysw_cp_free(ext->drag_start_cp);
            ext->drag_start_cp = NULL;
        }
        ext->dragging = false;
    } else {
        do_click(cpe);
    }
    ext->selected_step = NULL;
    lv_obj_invalidate(cpe);
}

static void on_signal_press_lost(lv_obj_t *cpe, void *param)
{
    ESP_LOGD(TAG, "on_signal_press_lost entered");
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    if (ext->dragging) {
        ext->dragging = false;
        if (ext->drag_start_cp) {
            uint32_t step_count = ysw_cp_get_step_count(ext->cp);
            for (uint32_t i = 0; i < step_count; i++) {
                ysw_step_t *step = ysw_cp_get_step(ext->cp, i);
                ysw_step_t *drag_start_step = ysw_cp_get_step(ext->drag_start_cp, i);
                *step = *drag_start_step;
            }
            ysw_cp_free(ext->drag_start_cp);
            ext->drag_start_cp = NULL;
        }
    }
    ext->selected_step = NULL;
    lv_obj_invalidate(cpe);
}

static void prepare_create(lv_obj_t *cpe, lv_point_t *point)
{
    metrics_t m;
    get_metrics(cpe, &m);
    if (point->y < m.cp_top + m.cp_height) {
        ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
        lv_coord_t adj_x = (point->x - m.cp_left) + ext->scroll_left;
        lv_coord_t adj_y = (point->y - m.cp_top);
        uint32_t step_index = adj_x / m.col_width;
        uint32_t row_index = adj_y / m.row_height;
        uint8_t degree = YSW_MIDI_UNPO - row_index;
        bool insert_after = (adj_x % m.col_width) > (m.col_width / 2);
        if (ysw_cp_get_step_count(ext->cp) > 0 && insert_after) {
            step_index++;
        }
        ESP_LOGD(TAG, "point->x=%d, y=%d, cp_left=%d, cp_top=%d, scroll_left=%d, col_width=%d, row_height=%d, insert_after=%d", point->x, point->y, m.cp_left, m.cp_top, ext->scroll_left, m.col_width, m.row_height, insert_after);
        fire_create(cpe, step_index, degree);
    }
}

static void on_signal_long_press(lv_obj_t *cpe, void *param)
{
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
            prepare_create(cpe, point);
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
    ext->scroll_left = 0;
    ext->drag_start_scroll_left = 0;
    ext->dragging = false;
    ext->long_press = false;
    ext->drag_start_cp = NULL;
    ext->metro_note = NULL;

    ext->bg_style = &lv_style_plain;
    ext->fg_style = &ysw_style_ei;
    ext->rs_style = &ysw_style_rn;
    ext->ss_style = &ysw_style_sn;
    ext->ms_style = &ysw_style_mn;

    ext->event_cb = NULL;

    lv_obj_set_signal_cb(cpe, signal_cb);
    lv_obj_set_design_cb(cpe, design_cb);
    lv_obj_set_click(cpe, true);
    lv_obj_set_style(cpe, ext->bg_style);

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
    ext->scroll_left = 0;
    // TODO: reinitialize rest of ext
    lv_obj_invalidate(cpe);
}

void ysw_lv_cpe_set_event_cb(lv_obj_t *cpe, ysw_lv_cpe_event_cb_t event_cb)
{
    LV_ASSERT_OBJ(cpe, LV_OBJX_NAME);
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    ext->event_cb = event_cb;
}

void ysw_lv_cpe_on_metro(lv_obj_t *cpe, note_t *metro_note)
{
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    if (metro_note != ext->metro_note) {
        ext->metro_note = metro_note;
        lv_obj_invalidate(cpe);
    }
}

void ysw_lv_cpe_ensure_visible(lv_obj_t *cpe, uint32_t first_step_index, uint32_t last_step_index)
{
    // NB: scroll_left is always <= 0
    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);
    metrics_t m;
    get_metrics(cpe, &m);
    lv_coord_t left = first_step_index * m.col_width;
    lv_coord_t right = (last_step_index + 1) * m.col_width;
    //ESP_LOGD(TAG, "first=%d, last=%d, col_width=%d, scroll_left=%d, left=%d, right=%d", first_step_index, last_step_index, m.col_width, ext->scroll_left, left, right);
    if (left < -ext->scroll_left) {
        ext->scroll_left = -left;
    } else if (right > -ext->scroll_left + m.cpe_width) {
        ext->scroll_left = m.cpe_width - right;
    }
    //ESP_LOGD(TAG, "new scroll_left=%d", ext->scroll_left);
}

