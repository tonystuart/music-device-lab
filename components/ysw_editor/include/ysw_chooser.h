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

typedef enum {
    YSW_CHOOSER_SORT_BY_NAME,
    YSW_CHOOSER_SORT_BY_SIZE,
    YSW_CHOOSER_SORT_BY_AGE,
} ysw_chooser_sort_t;

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
void ysw_chooser_hide_selection(ysw_chooser_t *chooser);
void ysw_chooser_show_selection(ysw_chooser_t *chooser, zm_section_x new_row);
void ysw_chooser_update_sections(ysw_chooser_t *chooser);
void ysw_chooser_sort(ysw_chooser_t *chooser, ysw_chooser_sort_t type);
void ysw_chooser_rename(ysw_chooser_t *chooser);
