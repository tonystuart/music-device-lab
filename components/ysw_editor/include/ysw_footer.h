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

lv_obj_t *ysw_footer_create(lv_obj_t *par);

void ysw_footer_set_key(lv_obj_t *footer, zm_key_signature_x key_index);

void ysw_footer_set_time(lv_obj_t *footer, zm_time_signature_x time_index);

void ysw_footer_set_tempo(lv_obj_t *footer, zm_tempo_x tempo);

void ysw_footer_set_duration(lv_obj_t *footer, zm_duration_t duration);

