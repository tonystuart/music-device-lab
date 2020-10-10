// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_heap.h"
#include "ysw_main_seq.h"
#include "ysw_main_synth.h"
#include "ysw_staff.h"
#include "ysw_synth_mod.h"
#include "zm_music.h"
#include "esp_log.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"
#include "termios.h"
#include "unistd.h"

#define TAG "MAIN"

#define YSW_MUSIC_PARTITION "/spiffs"

static zm_song_t *initialize_song(zm_music_t *music, uint32_t index)
{
    zm_song_t *song = ysw_array_get(music->songs, index);
    zm_small_t part_count = ysw_array_get_count(song->parts);

    for (zm_small_t i = 0; i < part_count; i++) {
        zm_part_t  *part = ysw_array_get(song->parts, i);
        zm_sample_t *sample = ysw_array_get(music->samples, part->pattern->sample_index);
        ysw_synth_mod_message_t message = {
            .type = YSW_SYNTH_MOD_SAMPLE_LOAD,
            .sample_load.index = part->pattern->sample_index,
            .sample_load.reppnt = sample->reppnt,
            .sample_load.replen = sample->replen,
            .sample_load.volume = sample->volume,
            .sample_load.pan = sample->pan,
        };
        snprintf(message.sample_load.name, sizeof(message.sample_load.name),
            "%s/samples/%s", YSW_MUSIC_PARTITION, sample->name);
        // TODO: figure out how to extend message for mod types
        ysw_main_synth_send((void*)&message);
    }

    return song;
}

static void play_song(ysw_array_t *array, uint8_t bpm)
{
    zm_large_t note_count = ysw_array_get_count(array);

    ysw_note_t *notes = ysw_heap_allocate(sizeof(ysw_note_t) * note_count);
    for (zm_large_t i = 0; i < note_count; i++) {
        notes[i] = *(ysw_note_t *)ysw_array_get(array, i);
    }

    ysw_seq_message_t message = {
        .type = YSW_SEQ_LOOP,
        .loop.loop = true,
    };

    ysw_main_seq_send(&message);

    message = (ysw_seq_message_t){
        .type = YSW_SEQ_PLAY,
        .play.notes = notes,
        .play.note_count = note_count,
        .play.tempo = bpm,
    };

    ysw_main_seq_send(&message);
}

static void play_zm_song_on_ysw_synth()
{
    //lv_obj_t *staff = ysw_staff_create(lv_scr_act(), NULL);
    //lv_obj_set_size(staff, 320, 240);
    //lv_obj_align(staff, NULL, LV_ALIGN_CENTER, 0, 0);

    ysw_main_synth_initialize();
    ysw_main_seq_initialize();

    zm_music_t *music = zm_read();
    zm_song_t *song = initialize_song(music, 0);
    ysw_array_t *array = zm_render_song(song);
    //ysw_staff_set_notes(staff, array);
    play_song(array, song->bpm);
}

#ifdef IDF_VER

#include "ysw_spiffs.h"

void app_main()
{
    ysw_spiffs_initialize(YSW_MUSIC_PARTITION);

    play_zm_song_on_ysw_synth();
}

#else

#include "ysw_lvgl.h"

int main(int argc, char *argv[])
{
    lv_init();
    ysw_lvgl_hal_init();

    play_zm_song_on_ysw_synth();

    while (1) {
        lv_task_handler();
        usleep(5 * 1000);
    }
}

#endif


