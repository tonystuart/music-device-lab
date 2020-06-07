// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// Global Settings Controller

#include "gsc.h"

#include "ysw_auto_play.h"
#include "ysw_lv_hpe.h"
#include "ysw_lv_cse.h"
#include "ysw_music.h"
#include "ysw_sdb.h"

#include "lvgl/lvgl.h"

#include "esp_log.h"

#define TAG "GSC"

static void on_ysw_lv_hpe_gs_scroll(void *context, bool auto_scroll)
{
    ysw_lv_hpe_gs.auto_scroll = auto_scroll;
}

static void on_ysw_lv_hpe_gs_play(void *context, ysw_auto_play_t auto_play)
{
    ysw_lv_hpe_gs.auto_play = auto_play;
}

static void on_ysw_lv_cse_gs_play_all(void *context, ysw_auto_play_t auto_play)
{
    ysw_lv_cse_gs.auto_play_all = auto_play;
}

static void on_ysw_lv_cse_gs_play_last(void *context, ysw_auto_play_t auto_play)
{
    ysw_lv_cse_gs.auto_play_last = auto_play;
}

void gsc_create(ysw_music_t *music)
{
    ysw_sdb_t *sdb = ysw_sdb_create("Global Settings", NULL);
    ysw_sdb_add_separator(sdb, "Chord Progression Settings");
    ysw_sdb_add_checkbox(sdb, "Auto Scroll", ysw_lv_hpe_gs.auto_scroll, on_ysw_lv_hpe_gs_scroll);
    ysw_sdb_add_choice(sdb, "Auto Play", ysw_lv_hpe_gs.auto_play, ysw_auto_play_options, on_ysw_lv_hpe_gs_play);
    ysw_sdb_add_separator(sdb, "Chord Style Settings");
    ysw_sdb_add_choice(sdb, "Auto Play\n(on Change)", ysw_lv_cse_gs.auto_play_all, ysw_auto_play_options, on_ysw_lv_cse_gs_play_all);
    ysw_sdb_add_choice(sdb, "Auto Play\n(on Click)", ysw_lv_cse_gs.auto_play_last, ysw_auto_play_options, on_ysw_lv_cse_gs_play_last);
}

