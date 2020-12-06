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
#include "zm_music.h"
#include "lvgl.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_FOOTER"

typedef struct {
    lv_cont_ext_t cont_ext; // base class data
    lv_obj_t *key_field;
    lv_obj_t *time_field;
    lv_obj_t *tempo_field;
    lv_obj_t *duration_field;
} ysw_footer_ext_t;

lv_obj_t *ysw_footer_create(lv_obj_t *par)
{
    lv_obj_t *footer = lv_cont_create(par, NULL);
    assert(footer);

    ysw_footer_ext_t *ext = lv_obj_allocate_ext_attr(footer, sizeof(ysw_footer_ext_t));
    assert(ext);

    memset(ext, 0, sizeof(ysw_footer_ext_t));

    ext->key_field = ysw_field_create(footer);
    ysw_field_set_name(ext->key_field, "Key");

    ext->time_field = ysw_field_create(footer);
    ysw_field_set_name(ext->time_field, "Time");

    ext->tempo_field = ysw_field_create(footer);
    ysw_field_set_name(ext->tempo_field, "Tempo");

    ext->duration_field = ysw_field_create(footer);
    ysw_field_set_name(ext->duration_field, "Duration");

    lv_cont_set_layout(footer, LV_LAYOUT_PRETTY_TOP);

    return footer;
}

void ysw_footer_set_key(lv_obj_t *footer, zm_key_signature_x key_index)
{
    const zm_key_signature_t *key_signature = zm_get_key_signature(key_index);
    ysw_footer_ext_t *ext = lv_obj_get_ext_attr(footer);
    ysw_field_set_value(ext->key_field, key_signature->label);
}

void ysw_footer_set_time(lv_obj_t *footer, zm_time_signature_x time_index)
{
    const zm_time_signature_t *time_signature = zm_get_time_signature(time_index);
    ysw_footer_ext_t *ext = lv_obj_get_ext_attr(footer);
    ysw_field_set_value(ext->time_field, time_signature->name);
}

void ysw_footer_set_tempo(lv_obj_t *footer, zm_tempo_t tempo_index)
{
    const zm_tempo_signature_t *tempo_signature = zm_get_tempo_signature(tempo_index);
    ysw_footer_ext_t *ext = lv_obj_get_ext_attr(footer);
    ysw_field_set_value(ext->tempo_field, tempo_signature->label);
}

void ysw_footer_set_duration(lv_obj_t *footer, zm_duration_t duration)
{
    const char *value = zm_get_duration_label(duration);
    ysw_footer_ext_t *ext = lv_obj_get_ext_attr(footer);
    ysw_field_set_value(ext->duration_field, value);
}

