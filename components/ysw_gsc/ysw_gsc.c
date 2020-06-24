// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// Global Settings Controller

#include "ysw_gsc.h"

#include "ysw_auto_play.h"
#include "ysw_hpe.h"
#include "ysw_cse.h"
#include "ysw_music.h"
#include "ysw_sdb.h"

#include "lvgl/lvgl.h"

#include "esp_log.h"

#define TAG "GSC"

static void on_ysw_hpe_gs_scroll(void *context, bool auto_scroll)
{
    ysw_hpe_gs.auto_scroll = auto_scroll;
}

static void on_ysw_hpe_gs_play_all(void *context, ysw_auto_play_t auto_play)
{
    ysw_hpe_gs.auto_play_all = auto_play;
}

static void on_ysw_hpe_gs_play_last(void *context, ysw_auto_play_t auto_play)
{
    ysw_hpe_gs.auto_play_last = auto_play;
}

static void on_multiple_selection(void *context, bool multiple_selection)
{
    ysw_hpe_gs.multiple_selection = multiple_selection;
}

void ysw_gsc_create(ysw_music_t *music)
{
    ysw_sdb_t *sdb = ysw_sdb_create(lv_scr_act(), "Global Settings", NULL);
    ysw_sdb_add_separator(sdb, "Chord Progression Settings");
    ysw_sdb_add_checkbox(sdb, "Auto Scroll", "",
            ysw_hpe_gs.auto_scroll, on_ysw_hpe_gs_scroll);
    ysw_sdb_add_choice(sdb, "Auto Play\n(on Change)",
            ysw_hpe_gs.auto_play_all, ysw_auto_play_options, on_ysw_hpe_gs_play_all);
    ysw_sdb_add_choice(sdb, "Auto Play\n(on Click)",
            ysw_hpe_gs.auto_play_last, ysw_auto_play_options, on_ysw_hpe_gs_play_last);
    ysw_sdb_add_choice(sdb, "Multiple Selection",
            ysw_hpe_gs.multiple_selection, "No\nYes", on_multiple_selection);
}

