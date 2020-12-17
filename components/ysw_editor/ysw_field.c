// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_field.h"
#include "ysw_style.h"
#include "lvgl.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_FIELD"

typedef struct {
    lv_cont_ext_t cont_ext; // base class data
    lv_obj_t *name;
    lv_obj_t *value;
} ysw_field_ext_t;

static lv_design_cb_t ysw_field_base_design;
static lv_signal_cb_t ysw_field_base_signal;

static lv_res_t ysw_field_on_signal(lv_obj_t *obj, lv_signal_t sign, void *param)
{
    return ysw_field_base_signal(obj, sign, param);
}

static lv_design_res_t ysw_field_on_design(lv_obj_t *obj, const lv_area_t *clip_area, lv_design_mode_t mode)
{
    return ysw_field_base_design(obj, clip_area, mode);
}

lv_obj_t *ysw_field_create(lv_obj_t *par)
{
    lv_obj_t *field = lv_cont_create(par, NULL);
    assert(field);

    ysw_field_ext_t *ext = lv_obj_allocate_ext_attr(field, sizeof(ysw_field_ext_t));
    assert(ext);

    if (ysw_field_base_signal == NULL) {
        ysw_field_base_signal = lv_obj_get_signal_cb(field);
    }

    if (ysw_field_base_design == NULL) {
        ysw_field_base_design = lv_obj_get_design_cb(field);
    }

    memset(ext, 0, sizeof(ysw_field_ext_t));

    lv_obj_set_signal_cb(field, ysw_field_on_signal);
    lv_obj_set_design_cb(field, ysw_field_on_design);

    ext->name = lv_label_create(field, NULL);

    ext->value = lv_label_create(field, NULL);

    ysw_style_field(ext->name, field);

    lv_cont_set_fit(field, LV_FIT_TIGHT);
    lv_cont_set_layout(field, LV_LAYOUT_COLUMN_LEFT);

    return field;
}

void ysw_field_set_name_text(lv_obj_t *field, const char *name)
{
    ysw_field_ext_t *ext = lv_obj_get_ext_attr(field);
    lv_label_set_text(ext->name, name);
}

void ysw_field_set_value_text(lv_obj_t *field, const char *value)
{
    ysw_field_ext_t *ext = lv_obj_get_ext_attr(field);
    lv_label_set_text(ext->value, value);
}

lv_obj_t *ysw_field_get_name_label(lv_obj_t *field)
{
    ysw_field_ext_t *ext = lv_obj_get_ext_attr(field);
    return ext->name;
}

lv_obj_t *ysw_field_get_value_label(lv_obj_t *field)
{
    ysw_field_ext_t *ext = lv_obj_get_ext_attr(field);
    return ext->value;
}

