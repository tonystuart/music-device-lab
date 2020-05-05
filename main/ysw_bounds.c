// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_bounds.h"
#include "esp_log.h"

#define TAG "YSW_BOUNDS"

#define BOUNDS_BORDER 5

ysw_bounds_t ysw_bounds_check(lv_area_t *area, lv_point_t *point)
{
    lv_coord_t x = point->x;
    lv_coord_t y = point->y;
    lv_coord_t top = area->y1 - BOUNDS_BORDER;
    lv_coord_t bottom = area->y2 + BOUNDS_BORDER;
    ysw_bounds_t bounds_type = YSW_BOUNDS_NONE;
    if (top <= y && y <= bottom) {
        lv_coord_t left = area->x1 - BOUNDS_BORDER;
        lv_coord_t middle_1 = area->x1 + BOUNDS_BORDER;
        lv_coord_t middle_2 = area->x2 - BOUNDS_BORDER;
        lv_coord_t right = area->x2 + BOUNDS_BORDER;
        if (middle_1 <= x && x <= middle_2) {
            bounds_type = YSW_BOUNDS_MIDDLE;
        } else if (left <= x && x <= middle_1) {
            bounds_type = YSW_BOUNDS_LEFT;
        } else if (middle_2 <= x && x <= right) {
            bounds_type = YSW_BOUNDS_RIGHT;
        }
    }
    ESP_LOGD(TAG, "ysw_bounds_check bounds_type=%d", bounds_type);
    return bounds_type;
}

bool ysw_bounds_check_point(lv_point_t *last_click, lv_point_t *point)
{
    lv_coord_t x = point->x;
    lv_coord_t y = point->y;
    return ((last_click->x - BOUNDS_BORDER) <= x && x <= (last_click->x + BOUNDS_BORDER)) &&
        ((last_click->y - BOUNDS_BORDER) <= y && y <= (last_click->y + BOUNDS_BORDER));
}

