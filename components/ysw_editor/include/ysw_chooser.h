// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "zm_music.h"
#include "lvgl.h"

typedef struct {
    zm_music_t *music;
    lv_obj_t *page;
    lv_obj_t *table;
    int current_row;
} ysw_chooser_t;

ysw_chooser_t *ysw_chooser_create(zm_music_t *music, zm_section_t *current_section);
void ysw_chooser_on_up(ysw_chooser_t *chooser);
void ysw_chooser_on_down(ysw_chooser_t *chooser);
zm_section_t *ysw_chooser_get_section(ysw_chooser_t *chooser);
void ysw_chooser_delete(ysw_chooser_t *chooser);
void ysw_chooser_select_row(ysw_chooser_t *chooser, zm_section_x new_row);
