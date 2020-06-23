// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_hpe.h"

#include "ysw_hp.h"
#include "ysw_style.h"
#include "ysw_ticks.h"

#include "lvgl.h"
#include "lv_debug.h"

#include "esp_log.h"

#include "math.h"
#include "stdio.h"
#include "stdlib.h"

#define TAG "YSW_HPE"
#define LV_OBJX_NAME "ysw_hpe"
#define MINIMUM_DRAG 10

ysw_hpe_gs_t ysw_hpe_gs = {
    .auto_scroll = true,
    .auto_play_all = YSW_AUTO_PLAY_OFF,
    .auto_play_last = YSW_AUTO_PLAY_PLAY,
    .multiple_selection = false,
};

typedef struct {
    lv_coord_t hpe_left;
    lv_coord_t hpe_top;
    lv_coord_t hpe_height;
    lv_coord_t hpe_width;
    lv_coord_t col_count;
    lv_coord_t col_width;
    lv_coord_t row_height;
    lv_coord_t hp_left;
    lv_coord_t hp_top;
    lv_coord_t hp_width;
    lv_coord_t hp_height;
    lv_coord_t min_scroll_left;
} metrics_t;

typedef struct {
    metrics_t m;
    ysw_hpe_ext_t *ext;
    ysw_ps_t *ps;
    uint32_t ps_index;
    uint32_t ps_count;
    uint32_t measure;
    uint8_t *spm;
    bool cs_displayed;
    lv_coord_t left;
    const lv_area_t *mask;
} dc_t;

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

static void get_metrics(lv_obj_t *hpe, metrics_t *m)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    uint32_t ps_count = ysw_hp_get_ps_count(ext->hp);

    m->hpe_left = hpe->coords.x1;
    m->hpe_top = hpe->coords.y1;

    m->hpe_height = lv_obj_get_height(hpe);
    m->hpe_width = lv_obj_get_width(hpe);

    m->col_count = ps_count > 16 ? 16 : ps_count > 0 ? ps_count : 1;
    m->col_width = m->hpe_width / m->col_count;
    m->row_height = m->hpe_height / 9; // 9 = header + 7 degrees + footer

    m->hp_left = m->hpe_left + ext->scroll_left;
    m->hp_top = m->hpe_top + m->row_height;

    m->hp_width = ps_count * m->col_width;
    m->hp_height = m->hpe_height - (2 * m->row_height);

    m->min_scroll_left = m->hpe_width - m->hp_width;
}

static void get_ps_info(lv_obj_t *hpe, uint32_t ps_index, lv_area_t *ret_area)
{
    metrics_t m;
    get_metrics(hpe, &m);

    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    ysw_ps_t *ps = ysw_hp_get_ps(ext->hp, ps_index);

    lv_coord_t left = m.hp_left + (ps_index * m.col_width);
    lv_coord_t cell_top = m.hp_top + ((YSW_MIDI_UNPO - ps->degree) * m.row_height);

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

static void draw_horizontals(dc_t *dc)
{
    lv_point_t top_left = {
        .x = dc->m.hpe_left,
        .y = dc->m.hp_top,
    };
    lv_point_t top_right = {
        .x = dc->m.hpe_left + dc->m.hpe_width,
        .y = dc->m.hp_top,
    };
    lv_draw_line(&top_left, &top_right, dc->mask, &line_dsc);

    lv_point_t bottom_left = {
        .x = dc->m.hpe_left,
        .y = dc->m.hp_top + dc->m.hp_height,
    };
    lv_point_t bottom_right = {
        .x = dc->m.hpe_left + dc->m.hpe_width,
        .y = dc->m.hp_top + dc->m.hp_height,
    };
    lv_draw_line(&bottom_left, &bottom_right, dc->mask, &line_dsc);
    lv_area_t background_area = {
        .x1 = dc->m.hpe_left,
        .y1 = dc->m.hp_top + 1,
        .x2 = dc->m.hpe_left + dc->m.hpe_width,
        .y2 = dc->m.hp_top + dc->m.hp_height - 1,
    };
    lv_draw_rect(&background_area, dc->mask, &even_rect_dsc);
}

static void draw_column_line(dc_t *dc)
{
    if (dc->ps_index) {
        lv_coord_t col_top = ysw_ps_is_new_measure(dc->ps) ? dc->m.hpe_top : dc->m.hp_top;
        lv_point_t top = {
            .x = dc->left,
            .y = col_top,
        };
        lv_point_t bottom = {
            .x = dc->left,
            .y = dc->m.hp_top + dc->m.hp_height,
        };
        lv_draw_line(&top, &bottom, dc->mask, &line_dsc);
    }
}

#if 0
static void draw_column_background(dc_t *dc)
{
    if (!dc->ps_index || ysw_ps_is_new_measure(dc->ps)) {
        lv_area_t background_area = {
            .x1 = dc->left,
            .y1 = dc->m.hp_top + 1,
            .x2 = dc->left + (dc->spm[dc->measure] * dc->m.col_width),
            .y2 = dc->m.hp_top + dc->m.hp_height - 1,
        };
        if (dc->even) {
            lv_draw_rect(&background_area, dc->mask, &even_rect_dsc);
        }
        dc->even = !dc->even;
    }
}
#endif

static void draw_column_heading(dc_t *dc)
{
    if (!dc->ps_index || ysw_ps_is_new_measure(dc->ps)) {
        lv_area_t heading_area = {
            .x1 = dc->left,
            .y1 = dc->m.hpe_top,
            .x2 = dc->left + (dc->spm[dc->measure] * dc->m.col_width),
            .y2 = dc->m.hp_top,
        };
        dc->measure++;
        lv_area_t heading_mask;
        if (_lv_area_intersect(&heading_mask, dc->mask, &heading_area)) {
            char buffer[32];
            ysw_itoa(dc->measure, buffer, sizeof(buffer));
            lv_draw_label(&heading_area, &heading_mask, &sn_label_dsc, buffer, NULL);
        }
    }
}

static void draw_ps(dc_t *dc)
{
    lv_coord_t cell_top = dc->m.hp_top + ((YSW_MIDI_UNPO - dc->ps->degree) * dc->m.row_height);
    lv_area_t cell_area = {
        .x1 = dc->left,
        .y1 = cell_top,
        .x2 = dc->left + dc->m.col_width,
        .y2 = cell_top + dc->m.row_height,
    };
    lv_area_t cell_mask;
    if (_lv_area_intersect(&cell_mask, dc->mask, &cell_area)) {
        if (ysw_ps_is_selected(dc->ps)) {
            lv_draw_rect(&cell_area, &cell_mask, &sel_sn_rect_dsc);
            lv_draw_label(&cell_area, &cell_mask, &sel_sn_label_dsc, key_labels[to_index(dc->ps->degree)], NULL);
        } else {
            lv_draw_rect(&cell_area, &cell_mask, &sn_rect_dsc);
            lv_draw_label(&cell_area, &cell_mask, &sn_label_dsc, key_labels[to_index(dc->ps->degree)], NULL);
        }
    }
}

static void draw_cs(dc_t *dc)
{
    lv_area_t footer_area = {
        .x1 = dc->m.hpe_left,
        .y1 = dc->m.hp_top + dc->m.hp_height,
        .x2 = dc->m.hpe_left + dc->m.hpe_width,
        .y2 = dc->m.hp_top + dc->m.hp_height + dc->m.row_height,
    };
    lv_area_t footer_mask;
    if (_lv_area_intersect(&footer_mask, dc->mask, &footer_area)) {
        lv_draw_label(&footer_area, &footer_mask, &sn_label_dsc, dc->ps->cs->name, NULL);
    }
}

static void draw_metro_marker(dc_t *dc)
{
    lv_coord_t left = dc->m.hp_left + (dc->ext->metro_marker * dc->m.col_width);
    lv_point_t top = {
        .x = left,
        .y = dc->m.hp_top,
    };
    lv_point_t bottom = {
        .x = left,
        .y = dc->m.hp_top + dc->m.hp_height,
    };
    lv_draw_line(&top, &bottom, dc->mask, &line_dsc);
}

static void draw_main(lv_obj_t *hpe, const lv_area_t *mask)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    uint32_t ps_count = ysw_hp_get_ps_count(ext->hp);
    uint8_t spm[ps_count];

    dc_t *dc = &(dc_t ) {
                .ext = ext,
                .ps_count = ps_count,
                .spm = spm,
                .mask = mask,
            };

    get_metrics(hpe, &dc->m);
    ysw_hp_get_pss_in_measures(dc->ext->hp, dc->spm, dc->ps_count); // TODO: rename ysw_hp_get_spm

    if (dc->ext->metro_marker != -1 && ysw_hpe_gs.auto_scroll) {
        ysw_hpe_ensure_visible(hpe, dc->ext->metro_marker, dc->ext->metro_marker);
    }

    draw_horizontals(dc);

    for (dc->ps_index = 0; dc->ps_index < dc->ps_count; dc->ps_index++) {
        dc->ps = ysw_hp_get_ps(dc->ext->hp, dc->ps_index);
        dc->left = dc->m.hp_left + (dc->ps_index * dc->m.col_width);
        draw_column_line(dc);
        draw_column_heading(dc);
        draw_ps(dc);
        if (!dc->cs_displayed && ysw_ps_is_selected(dc->ps)) {
            draw_cs(dc);
            dc->cs_displayed = true;
        }
    }

    if (dc->ext->metro_marker != -1) {
        draw_metro_marker(dc);
    }
}

static lv_design_res_t design_cb(lv_obj_t *hpe, const lv_area_t *mask, lv_design_mode_t mode)
{
    bool result = true;
    switch (mode) {
        case LV_DESIGN_COVER_CHK:
            result = super_design_cb(hpe, mask, mode);
            break;
        case LV_DESIGN_DRAW_MAIN:
            super_design_cb(hpe, mask, mode);
            draw_main(hpe, mask);
            break;
        case LV_DESIGN_DRAW_POST:
            break;
    }
    return result;
}

static void fire_create(lv_obj_t *hpe, uint32_t ps_index, uint8_t degree)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    if (ext->create_cb) {
        ext->create_cb(ext->context, ps_index, degree);
    }
}

static void fire_edit(lv_obj_t *hpe, ysw_ps_t *ps)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    if (ext->edit_cb) {
        ext->edit_cb(ext->context, ps);
    }
}

static void fire_select(lv_obj_t *hpe, ysw_ps_t *ps)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    if (ext->select_cb) {
        ext->select_cb(ext->context, ps);
    }
}

static void fire_deselect(lv_obj_t *hpe, ysw_ps_t *ps)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    if (ext->deselect_cb) {
        ext->deselect_cb(ext->context, ps);
    }
}

static void fire_drag_end(lv_obj_t *hpe)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    if (ext->drag_end_cb) {
        ext->drag_end_cb(ext->context);
    }
}

static uint32_t count_selected_ps(lv_obj_t *hpe)
{
    uint32_t count = 0;
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    uint32_t ps_count = ysw_hp_get_ps_count(ext->hp);
    for (int i = 0; i < ps_count; i++) {
        ysw_ps_t *ps = ysw_hp_get_ps(ext->hp, i);
        if (ysw_ps_is_selected(ps)) {
            count++;
        }
    }
    return count;
}

static void select_ps(lv_obj_t *hpe, ysw_ps_t *ps)
{
    ysw_ps_select(ps, true);
    fire_select(hpe, ps);
}

static void deselect_ps(lv_obj_t *hpe, ysw_ps_t *ps)
{
    ysw_ps_select(ps, false);
    fire_deselect(hpe, ps);
}

static void deselect_all(lv_obj_t *hpe)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    uint32_t ps_count = ysw_hp_get_ps_count(ext->hp);
    for (int i = 0; i < ps_count; i++) {
        ysw_ps_t *ps = ysw_hp_get_ps(ext->hp, i);
        if (ysw_ps_is_selected(ps)) {
            deselect_ps(hpe, ps);
        }
    }
}

static void prepare_create(lv_obj_t *hpe, lv_point_t *point)
{
    metrics_t m;
    get_metrics(hpe, &m);
    if (point->y < m.hp_top + m.hp_height) {
        ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
        lv_coord_t adj_x = (point->x - m.hp_left) + ext->scroll_left;
        lv_coord_t adj_y = (point->y - m.hp_top);
        uint32_t ps_index = adj_x / m.col_width;
        uint32_t row_index = adj_y / m.row_height;
        uint8_t degree = YSW_MIDI_UNPO - row_index;
        bool insert_after = (adj_x % m.col_width) > (m.col_width / 2);
        if (ysw_hp_get_ps_count(ext->hp) > 0 && insert_after) {
            ps_index++;
        }
        if (!ysw_hpe_gs.multiple_selection) {
            deselect_all(hpe);
        }
        fire_create(hpe, ps_index, degree);
    }
}

static void scroll_horizontally(lv_obj_t *hpe, lv_coord_t x)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    metrics_t m;
    get_metrics(hpe, &m);
    lv_coord_t new_scroll_left = ext->drag_start_scroll_left + x;
    if (new_scroll_left < m.min_scroll_left) {
        new_scroll_left = m.min_scroll_left;
    }
    if (new_scroll_left > 0) {
        new_scroll_left = 0;
    }
    ext->scroll_left = new_scroll_left;
}

static void drag_vertically(lv_obj_t *hpe, ysw_ps_t *ps, ysw_ps_t *drag_start_ps, lv_coord_t y)
{
    metrics_t m;
    get_metrics(hpe, &m);
    uint8_t new_degree = drag_start_ps->degree - (y / m.row_height);
    if (new_degree >= 1 && new_degree <= YSW_MIDI_UNPO) {
        ps->degree = new_degree;
    }
}

static void capture_drag(lv_obj_t *hpe, lv_coord_t x, lv_coord_t y)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    if (!ext->dragging && !ext->scrolling) {
        bool drag_x = abs(x) > MINIMUM_DRAG;
        bool drag_y = abs(y) > MINIMUM_DRAG;
        if (ext->clicked_ps) {
            ext->dragging = drag_x || drag_y;
            if (ext->dragging) {
                if (!ysw_ps_is_selected(ext->clicked_ps)) {
                    if (count_selected_ps(hpe)) {
                        deselect_all(hpe);
                    }
                    select_ps(hpe, ext->clicked_ps);
                }
                ext->drag_start_hp = ysw_hp_copy(ext->hp);
            }
        } else {
            ext->scrolling = drag_x || drag_y;
            if (ext->scrolling) {
                ext->drag_start_scroll_left = ext->scroll_left;
            }
        }
    }
}

static void capture_click(lv_obj_t *hpe, lv_point_t *point)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    ext->clicked_ps = NULL;
    ext->click_point = *point;
    uint32_t ps_count = ysw_hp_get_ps_count(ext->hp);
    for (uint8_t i = 0; i < ps_count; i++) {
        lv_area_t ps_area;
        ysw_ps_t *ps = ysw_hp_get_ps(ext->hp, i);
        get_ps_info(hpe, i, &ps_area);
        if ((ps_area.x1 <= point->x && point->x <= ps_area.x2) &&
                (ps_area.y1 <= point->y && point->y <= ps_area.y2)) {
            ext->clicked_ps = ps;
            if (ysw_ps_is_selected(ps)) {
                // return on first selected ps, otherwise pick last one
                return;
            }
        }
    }
}

static void on_signal_pressed(lv_obj_t *hpe, void *param)
{
    lv_indev_t *indev_act = (lv_indev_t*)param;
    lv_indev_proc_t *proc = &indev_act->proc;
    lv_point_t *point = &proc->types.pointer.act_point;
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    if (ext->press_lost) {
        // user has dragged back into hpe widget
        ext->press_lost = false;
    } else {
        // user has clicked on hpe widget
        ext->dragging = false;
        ext->scrolling = false;
        ext->long_press = false;
        ext->press_lost = false;
        capture_click(hpe, point);
    }
}

static void on_signal_pressing(lv_obj_t *hpe, void *param)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    lv_indev_t *indev_act = (lv_indev_t*)param;
    lv_indev_proc_t *proc = &indev_act->proc;
    lv_point_t *point = &proc->types.pointer.act_point;
    lv_coord_t x = point->x - ext->click_point.x;
    lv_coord_t y = point->y - ext->click_point.y;
    capture_drag(hpe, x, y);
    if (ext->dragging) {
        uint32_t ps_count = ysw_hp_get_ps_count(ext->hp);
        for (int i = 0; i < ps_count; i++) {
            ysw_ps_t *ps = ysw_hp_get_ps(ext->hp, i);
            ysw_ps_t *drag_start_ps = ysw_hp_get_ps(ext->drag_start_hp, i);
            if (ysw_ps_is_selected(ps)) {
                drag_vertically(hpe, ps, drag_start_ps, y);
            }
        }
        lv_obj_invalidate(hpe);
    } else if (ext->scrolling) {
        scroll_horizontally(hpe, x);
        lv_obj_invalidate(hpe);
    }
}

static void on_signal_released(lv_obj_t *hpe, void *param)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    if (ext->dragging) {
        fire_drag_end(hpe);
        ysw_hp_free(ext->drag_start_hp);
    } else if (ext->scrolling) {
        // reset flag below for all cases
    } else if (ext->long_press) {
        // reset flag below for all cases
    } else if (ext->press_lost) {
        // reset flag below for all cases
    } else if (ext->clicked_ps) {
        bool selected = ysw_ps_is_selected(ext->clicked_ps);
        if (selected) {
            deselect_ps(hpe, ext->clicked_ps);
        } else {
            if (!ysw_hpe_gs.multiple_selection) {
                deselect_all(hpe);
            }
            select_ps(hpe, ext->clicked_ps);
        }
    } else {
        deselect_all(hpe);
    }
    ext->dragging = false;
    ext->drag_start_hp = NULL;
    ext->long_press = false;
    ext->press_lost = false;
    ext->clicked_ps = NULL;
    ext->scrolling = false;
    lv_obj_invalidate(hpe);
}

static void on_signal_press_lost(lv_obj_t *hpe, void *param)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    ext->press_lost = true;
}

static void on_signal_long_press(lv_obj_t *hpe, void *param)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    if (!ext->dragging && !ext->scrolling) {
        lv_indev_t *indev_act = (lv_indev_t*)param;
        lv_indev_wait_release(indev_act);
        if (ext->clicked_ps) {
            fire_edit(hpe, ext->clicked_ps);
        } else {
            lv_indev_proc_t *proc = &indev_act->proc;
            lv_point_t *point = &proc->types.pointer.act_point;
            prepare_create(hpe, point);
        }
        ext->long_press = true;
    }
}

static lv_res_t signal_cb(lv_obj_t *hpe, lv_signal_t signal, void *param)
{
    lv_res_t res = super_signal_cb(hpe, signal, param);
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
                on_signal_pressed(hpe, param);
                break;
            case LV_SIGNAL_PRESSING:
                on_signal_pressing(hpe, param);
                break;
            case LV_SIGNAL_RELEASED:
                on_signal_released(hpe, param);
                break;
            case LV_SIGNAL_PRESS_LOST:
                on_signal_press_lost(hpe, param);
                break;
            case LV_SIGNAL_LONG_PRESS:
                on_signal_long_press(hpe, param);
                break;
        }
    }
    return res;
}

lv_obj_t* ysw_hpe_create(lv_obj_t *par, void *context)
{
    lv_obj_t *hpe = lv_obj_create(par, NULL);
    LV_ASSERT_MEM(hpe);
    if (hpe == NULL) {
        return NULL;
    }

    if (super_signal_cb == NULL) {
        super_signal_cb = lv_obj_get_signal_cb(hpe);
    }

    if (super_design_cb == NULL) {
        super_design_cb = lv_obj_get_design_cb(hpe);
    }

    ysw_hpe_ext_t *ext = lv_obj_allocate_ext_attr(hpe, sizeof(ysw_hpe_ext_t));
    LV_ASSERT_MEM(ext);
    if (ext == NULL) {
        return NULL;
    }

    *ext = (ysw_hpe_ext_t ) {
                .metro_marker = -1,
                //v7: .bg_style = &lv_style_plain,
                //v7: .fg_style = &ysw_style_ei,
                //v7: .rs_style = &ysw_style_rn,
                //v7: .ss_style = &ysw_style_sn,
                //v7: .ms_style = &ysw_style_mn,
                .context = context,
            };

//v7: lv_obj_set_style(hpe, ext->bg_style);
    lv_obj_set_signal_cb(hpe, signal_cb);
    lv_obj_set_design_cb(hpe, design_cb);
    lv_obj_set_click(hpe, true);

    return hpe;
}

void ysw_hpe_set_hp(lv_obj_t *hpe, ysw_hp_t *hp)
{
    LV_ASSERT_OBJ(hpe, LV_OBJX_NAME);
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    if (ext->hp == hp) {
        return;
    }
    ext->hp = hp;
    ext->scroll_left = 0;
// TODO: reinitialize rest of ext
    lv_obj_invalidate(hpe);
}

void ysw_hpe_set_create_cb(lv_obj_t *hpe, void *cb)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    ext->create_cb = cb;
}

void ysw_hpe_set_edit_cb(lv_obj_t *hpe, void *cb)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    ext->edit_cb = cb;
}

void ysw_hpe_set_select_cb(lv_obj_t *hpe, void *cb)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    ext->select_cb = cb;
}

void ysw_hpe_set_deselect_cb(lv_obj_t *hpe, void *cb)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    ext->deselect_cb = cb;
}

void ysw_hpe_set_drag_end_cb(lv_obj_t *hpe, void *cb)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    ext->drag_end_cb = cb;
}

void ysw_hpe_on_metro(lv_obj_t *hpe, ysw_note_t *metro_note)
{
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    ESP_LOGD(TAG, "metro_note=%p", metro_note);
    int32_t metro_marker = metro_note ? metro_note->velocity : -1;
    ESP_LOGD(TAG, "metro_marker=%d", metro_marker);
    if (metro_marker != ext->metro_marker) {
        ext->metro_marker = metro_marker;
        lv_obj_invalidate(hpe);
    }
}

void ysw_hpe_ensure_visible(lv_obj_t *hpe, uint32_t first_ps_index, uint32_t last_ps_index)
{
// NB: scroll_left is always <= 0
    ysw_hpe_ext_t *ext = lv_obj_get_ext_attr(hpe);
    metrics_t m;
    get_metrics(hpe, &m);
    lv_coord_t left = first_ps_index * m.col_width;
    lv_coord_t right = (last_ps_index + 1) * m.col_width;
//ESP_LOGD(TAG, "first=%d, last=%d, col_width=%d, scroll_left=%d, left=%d, right=%d", first_ps_index, last_ps_index, m.col_width, ext->scroll_left, left, right);
    if (left < -ext->scroll_left) {
        ext->scroll_left = -left;
    } else if (right > -ext->scroll_left + m.hpe_width) {
        ext->scroll_left = m.hpe_width - right;
    }
//ESP_LOGD(TAG, "new scroll_left=%d", ext->scroll_left);
}

