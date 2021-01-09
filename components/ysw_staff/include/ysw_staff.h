// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "ysw_array.h"
#include "zm_music.h"
#include "lvgl.h"

typedef enum {
    YSW_STAFF_DIRECT,
    YSW_STAFF_SCROLL,
} ysw_staff_position_type_t;

typedef struct {
    uint32_t position;
    zm_section_t *section;
    zm_key_signature_x key;
    lv_coord_t staff_head;
    lv_coord_t start_x;
} ysw_staff_ext_t;

enum {
    YSW_STAFF_PART_MAIN = LV_OBJ_PART_MAIN,
};

typedef uint8_t ysw_staff_role_t;

lv_obj_t *ysw_staff_create(lv_obj_t *par);
void ysw_staff_set_section(lv_obj_t *staff, zm_section_t *section);
void ysw_staff_set_position(lv_obj_t *staff, zm_step_x new_position, ysw_staff_position_type_t type);
void ysw_staff_invalidate(lv_obj_t *staff);
zm_section_t *ysw_staff_get_section(lv_obj_t *staff);
uint32_t ysw_staff_get_position(lv_obj_t *staff);

#ifdef __cplusplus
} /* extern "C" */
#endif

