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

lv_obj_t *ysw_field_create(lv_obj_t *par);

void ysw_field_set_name(lv_obj_t *field, const char *name);

void ysw_field_set_value(lv_obj_t *field, const char *value);

