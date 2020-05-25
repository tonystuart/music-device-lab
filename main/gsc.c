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

#include "ysw_lv_cpe.h"
#include "ysw_lv_cse.h"
#include "ysw_music.h"
#include "ysw_sdb.h"

#include "lvgl/lvgl.h"

#include "esp_log.h"

#define TAG "GSC"

static void on_ysw_lv_cpe_gs_scroll(bool new_value)
{
    ysw_lv_cpe_gs.auto_scroll = new_value;
}

static void on_ysw_lv_cpe_gs_play(bool new_value)
{
    ysw_lv_cpe_gs.auto_play = new_value;
}

static void on_ysw_lv_cse_gs_play(bool new_value)
{
    ysw_lv_cse_gs.auto_play = new_value;
}

void gsc_create(ysw_music_t *music)
{
    ysw_sdb_t *sdb = ysw_sdb_create("Global Settings");
    ysw_sdb_add_separator(sdb, "Chord Progression Settings");
    ysw_sdb_add_checkbox(sdb, "Auto Scroll", ysw_lv_cpe_gs.auto_scroll, on_ysw_lv_cpe_gs_scroll);
    ysw_sdb_add_checkbox(sdb, "Auto Play", ysw_lv_cpe_gs.auto_play, on_ysw_lv_cpe_gs_play);
    ysw_sdb_add_separator(sdb, "Chord Style Settings");
    ysw_sdb_add_checkbox(sdb, "Auto Play", ysw_lv_cse_gs.auto_play, on_ysw_lv_cse_gs_play);
}

