// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_header.h"
#include "ysw_field.h"
#include "lvgl.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_HEADER"

typedef struct {
    lv_cont_ext_t cont_ext; // base class data
    lv_obj_t *mode_field;
    lv_obj_t *program_field;
} ysw_header_ext_t;

lv_obj_t *ysw_header_create(lv_obj_t *par)
{
    lv_obj_t *header = lv_cont_create(par, NULL);
    assert(header);

    ysw_header_ext_t *ext = lv_obj_allocate_ext_attr(header, sizeof(ysw_header_ext_t));
    assert(ext);

    memset(ext, 0, sizeof(ysw_header_ext_t));

    ext->mode_field = ysw_field_create(header);
    ysw_field_set_name_text(ext->mode_field, "Mode");

    ext->program_field = ysw_field_create(header);
    lv_cont_set_layout(ext->program_field, LV_LAYOUT_COLUMN_RIGHT);
    ysw_field_set_name_text(ext->program_field, "Program");

    lv_obj_t *value_label = ysw_field_get_value_label(ext->program_field);
    lv_label_set_long_mode(value_label, LV_LABEL_LONG_CROP);
    lv_label_set_align(value_label, LV_LABEL_ALIGN_RIGHT);

    lv_cont_set_layout(header, LV_LAYOUT_PRETTY_TOP);

    return header;
}

void ysw_header_set_mode(lv_obj_t *header, const char *name, const char *value)
{
    ysw_header_ext_t *ext = lv_obj_get_ext_attr(header);
    ysw_field_set_name_text(ext->mode_field, name);
    ysw_field_set_value_text(ext->mode_field, value);

    // Resize width of program's value label to what's left after setting mode's value
    lv_obj_t *mode_value_label = ysw_field_get_value_label(ext->mode_field);
    lv_coord_t mode_width = lv_obj_get_width(mode_value_label);
    lv_coord_t program_width = 315 - mode_width;

    lv_obj_t *program_value_label = ysw_field_get_value_label(ext->program_field);
    lv_obj_set_width(program_value_label, program_width);
}

void ysw_header_set_program(lv_obj_t *header, const char *program_name)
{
    ysw_header_ext_t *ext = lv_obj_get_ext_attr(header);
    ysw_field_set_value_text(ext->program_field, program_name);
}

