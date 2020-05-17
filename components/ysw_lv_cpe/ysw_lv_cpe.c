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
#include "ysw_cn.h"
#include "ysw_lv_styles.h"
#include "ysw_ticks.h"

#include "lvgl.h"

#include "esp_log.h"

#include "math.h"
#include "stdio.h"

#define TAG "YSW_LV_CPE"

#define LV_OBJX_NAME "ysw_lv_cpe"

static lv_design_cb_t super_design_cb;
static lv_signal_cb_t super_signal_cb;

static void draw_main(lv_obj_t *cpe, const lv_area_t *mask, lv_design_mode_t mode)
{
    super_design_cb(cpe, mask, mode);

    ysw_lv_cpe_ext_t *ext = lv_obj_get_ext_attr(cpe);

    if (!ext->cp) {
        return;
    }

    lv_coord_t h = lv_obj_get_height(cpe);
    lv_coord_t w = lv_obj_get_width(cpe);

    lv_coord_t x = cpe->coords.x1;
    lv_coord_t y = cpe->coords.y1;

    lv_area_t cn_area = {
        .x1 = 0,
        .y1 = 50,
        .x2 = 300,
        .y2 = 100
    };
    //get_cn_info(cpe, cn, &cn_area, NULL, &octave);

    lv_area_t cn_mask;

    if (lv_area_intersect(&cn_mask, mask, &cn_area)) {
            lv_point_t offset = {
                .x = 0,
                .y = 0, // e.g. to center vertically
            };
        lv_draw_label(&cn_area,
                &cn_mask,
                &lv_style_plain,
                lv_style_plain.text.opa,
                "Hello, world",
                LV_TXT_FLAG_EXPAND | LV_TXT_FLAG_CENTER,
                &offset,
                NULL,
                NULL,
                LV_BIDI_DIR_LTR);
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
                break;
            case LV_SIGNAL_PRESSING:
                break;
            case LV_SIGNAL_RELEASED:
                break;
            case LV_SIGNAL_PRESS_LOST:
                break;
            case LV_SIGNAL_LONG_PRESS:
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
