// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_event.h"
#include "ysw_sequencer.h"
#include "ysw_shell.h"
#include "zm_music.h"
#include "esp_log.h"
#include "stdlib.h"

#define TAG "YSW_MAIN"

extern void ysw_main_init_device(ysw_bus_t *bus, zm_music_t *music);

void ysw_main_create()
{
    ysw_bus_t *bus = ysw_event_create_bus();
    zm_music_t *music = zm_load_music();
    ysw_main_init_device(bus, music);
    ysw_sequencer_create_task(bus);
    ysw_shell_create(bus, music);
}

