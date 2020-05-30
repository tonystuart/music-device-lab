// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "lvgl.h"

typedef enum {
    YSW_BOUNDS_NONE = 0,
    YSW_BOUNDS_LEFT,
    YSW_BOUNDS_MIDDLE,
    YSW_BOUNDS_RIGHT,
} ysw_bounds_t;

ysw_bounds_t ysw_bounds_check(lv_area_t *area, lv_point_t *point);
bool ysw_bounds_check_point(lv_point_t *click_point, lv_point_t *point);
