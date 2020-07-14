// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_hp.h"
#include "lvgl.h"

lv_obj_t* ysw_hpp_create(lv_obj_t *par);
void ysw_hpp_set_hp(lv_obj_t *hpp, ysw_hp_t *hp);
