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
#include "../lv_conf_internal.h"
#include "../lv_core/lv_obj.h"

typedef struct {
    ysw_array_t *notes;
} ysw_staff_ext_t;

enum {
    YSW_STAFF_PART_MAIN = LV_OBJ_PART_MAIN,
};

typedef uint8_t ysw_staff_part_t;

lv_obj_t *ysw_staff_create(lv_obj_t *par, const lv_obj_t *copy);
void ysw_staff_set_notes(lv_obj_t *staff, ysw_array_t *notes);
ysw_array_t *ysw_staff_get_notes(const lv_obj_t *staff);

#ifdef __cplusplus
} /* extern "C" */
#endif

