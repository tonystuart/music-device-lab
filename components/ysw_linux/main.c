// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#define LINUX_MAIN 2

#if LINUX_MAIN == 1

#include "ysw_csl.h"
#include "ysw_hpl.h"
#include "ysw_main_bus.h"
#include "ysw_main_seq.h"
#include "ysw_main_synth.h"
#include "ysw_mfr.h"
#include "ysw_music.h"
#include "ysw_style.h"

#include <stdlib.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include "lvgl/lvgl.h"
#include "lv_drivers/display/monitor.h"
#include "lv_drivers/indev/mouse.h"
#include "lv_examples/lv_examples.h"
#include "esp_log.h"

#define TAG "MAIN"

static void hal_init(void);
static int tick_thread(void *data);
static void memory_monitor(lv_task_t *param);

static ysw_music_t *music;

static void event_handler(lv_obj_t *btn, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        const char *text = lv_list_get_btn_text(btn);
        if (strcmp(text, "Chords") == 0) {
            //ysw_csc_create(music, 0);
            ysw_csl_create(music, 0);
        } else if (strcmp(text, "Progressions") == 0) {
            ysw_hpl_create(music, 0);
        }
    }
}

static void create_dashboard(void)
{
    lv_obj_t *dashboard = lv_list_create(lv_scr_act(), NULL);
    lv_obj_set_size(dashboard, 160, 200);
    lv_obj_align(dashboard, NULL, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *chords = lv_list_add_btn(dashboard, LV_SYMBOL_AUDIO, "Chords");
    lv_obj_set_event_cb(chords, event_handler);

    lv_obj_t *progressions = lv_list_add_btn(dashboard, LV_SYMBOL_BELL, "Progressions");
    lv_obj_set_event_cb(progressions, event_handler);
}

// This is a copy of a function in player/main/main.c
static void play_hp_loop(ysw_music_t *music, uint32_t hp_index)
{
    ysw_seq_message_t message = {
        .type = YSW_SEQ_LOOP,
        .loop.loop = true,
    };

    ysw_main_seq_send(&message);

    ysw_hp_t *hp = ysw_music_get_hp(music, hp_index);

    uint32_t note_count = 0;
    ysw_note_t *notes = ysw_hp_get_notes(hp, &note_count);

    message = (ysw_seq_message_t){
        .type = YSW_SEQ_PLAY,
        .play.notes = notes,
        .play.note_count = note_count,
        .play.tempo = hp->tempo,
    };

    ysw_main_seq_send(&message);
}

int main(int argc, char **argv)
{
    lv_init();
    hal_init();

    ysw_main_bus_create();
    ysw_main_synth_initialize();
    ysw_main_seq_initialize();
    ysw_style_initialize();

    FILE *music_file = fopen("music.csv", "r");
    music = ysw_mfr_read_from_file(music_file);

    uint32_t cs_count = ysw_music_get_cs_count(music);
    ESP_LOGD(TAG, "cs_count=%d", cs_count);

    //lv_demo_widgets();
    create_dashboard();
    //play_hp_loop(music, 1);

    while (1) {
        lv_task_handler();
        usleep(5 * 1000);
    }

    return 0;
}

static void hal_init(void)
{
    monitor_init();

    static lv_disp_buf_t disp_buf1;
    static lv_color_t buf1_1[LV_HOR_RES_MAX * 120];
    lv_disp_buf_init(&disp_buf1, buf1_1, NULL, LV_HOR_RES_MAX * 120);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.buffer = &disp_buf1;
    disp_drv.flush_cb = monitor_flush;
    lv_disp_drv_register(&disp_drv);

    mouse_init();
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv); /*Basic initialization*/
    indev_drv.type = LV_INDEV_TYPE_POINTER;

    indev_drv.read_cb = mouse_read;
    lv_indev_t *mouse_indev = lv_indev_drv_register(&indev_drv);

    LV_IMG_DECLARE(mouse_cursor_icon); /*Declare the image file.*/
    lv_obj_t *cursor_obj = lv_img_create(lv_scr_act(), NULL); /*Create an image object for the cursor */
    lv_img_set_src(cursor_obj, &mouse_cursor_icon); /*Set the image source*/
    lv_indev_set_cursor(mouse_indev, cursor_obj); /*Connect the image  object to the driver*/

    SDL_CreateThread(tick_thread, "tick", NULL);

    //lv_task_create(memory_monitor, 5000, LV_TASK_PRIO_MID, NULL);
}

static int tick_thread(void *data)
{
    while (1) {
        SDL_Delay(5); /*Sleep for 5 millisecond*/
        lv_tick_inc(5); /*Tell LittelvGL that 5 milliseconds were elapsed*/
    }

    return 0;
}

UNUSED
static void memory_monitor(lv_task_t *param)
{
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    printf("used: %6d (%3d %%), frag: %3d %%, biggest free: %6d\n",
            (int)mon.total_size - mon.free_size, mon.used_pct, mon.frag_pct,
            (int)mon.free_biggest_size);
}

#elif LINUX_MAIN == 2

#include "hxcmod.h"
#include "ysw_alsa.h"
#include "ysw_heap.h"
#include "ysw_music.h"
#include "esp_log.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"
#include "termios.h"
#include "unistd.h"

#define TAG "MOD_MAIN"

#define YSW_MUSIC_MOD YSW_MUSIC_PARTITION "/music.mod"

typedef struct {
    char input_char;
    const char *note_name;
    int note_period;
} note_map_t;

static note_map_t note_map[] = {
    { 'q', "C-2", 428 },
    { 'w', "D-2", 381 },
    { 'e', "E-2", 339 },
    { 'r', "F-2", 320 },
    { 't', "G-2", 285 },
    { 'y', "A-2", 254 },
    { 'u', "B-2", 226 },
};

#define NOTE_MAP_COUNT (sizeof(note_map) / sizeof(note_map_t))

static modcontext *modctx;

static int8_t find_note(int8_t input_char)
{
    for (int8_t i = 0; i < NOTE_MAP_COUNT; i++) {
        if (note_map[i].input_char == input_char) {
            return i;
        }
    }
    return -1;
}

// NB: len is in bytes (typically 512 when called from a2dp_source)
static int32_t data_cb(uint8_t *data, int32_t len)
{
    if (len < 0 || data == NULL) {
        return 0;
    }
    hxcmod_fillbuffer(modctx, (msample*)data, len / 4);
    return len;
}

static void* alsa_thread(void *p)
{
    ysw_alsa_initialize(data_cb);
    return NULL;
}

int main(int argc, char *argv[])
{
    ESP_LOGD(TAG, "calling ysw_heap_allocate for modcontext, size=%d", sizeof(modcontext));
    modctx = ysw_heap_allocate(sizeof(modcontext));

    ESP_LOGD(TAG, "calling hxcmod_init, modctx=%p", modctx);
    hxcmod_init(modctx);

    ESP_LOGD(TAG, "calling stat, file=%s", YSW_MUSIC_MOD);
    struct stat sb;
    int rc = stat(YSW_MUSIC_MOD, &sb);
    if (rc == -1) {
        ESP_LOGE(TAG, "stat failed, file=%s", YSW_MUSIC_MOD);
        abort();
    }

    int mod_data_size = sb.st_size;

    ESP_LOGD(TAG, "calling ysw_heap_allocate for mod_data, size=%d", mod_data_size);
    void *mod_data = ysw_heap_allocate(mod_data_size);

    int fd = open(YSW_MUSIC_MOD, O_RDONLY);
    if (fd == -1) {
        ESP_LOGE(TAG, "open failed, file=%s", YSW_MUSIC_MOD);
        abort();
    }

    rc = read(fd, mod_data, mod_data_size);
    if (rc != mod_data_size) {
        ESP_LOGE(TAG, "read failed, rc=%d, mod_data_size=%d", rc, mod_data_size);
        abort();
    }

    rc = close(fd);
    if (rc == -1) {
        ESP_LOGE(TAG, "close failed");
        abort();
    }

    hxcmod_load(modctx, mod_data, mod_data_size);

    pthread_t p;
    pthread_create(&p, NULL, &alsa_thread, NULL);

    int c;
    extern void play_note(modcontext *modctx, int period);

    while ((c = getchar()) != -1) {
        int note_index = find_note(c);
        if (note_index != -1) {
            int note_period = note_map[note_index].note_period;
            play_note(modctx, note_period);
            ESP_LOGD(TAG, "input_char=%c, note_index=%d, note_name=%s, note_period=%d",
                    c, note_index, note_map[note_index].note_name, note_period);
        }
    }
}

#elif LINUX_MAIN == 3

#include "hxcmod.h"
#include "ysw_alsa.h"
#include "ysw_heap.h"
#include "ysw_music.h"
#include "zm_music.h"
#include "ysw_main_seq.h"
#include "ysw_main_synth.h"
#include "esp_log.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "pthread.h"
#include "stdio.h"
#include "stdlib.h"
#include "termios.h"
#include "unistd.h"

#define TAG "MOD_MAIN"

int main(int argc, char *argv[])
{
    ysw_main_synth_initialize();
    ysw_main_seq_initialize();

    zm_music_t *music = zm_read();
    zm_pattern_t *pattern = ysw_array_get(music->patterns, 0);
    ysw_array_t *array = zm_render_pattern(pattern);

    zm_large_t note_count = ysw_array_get_count(array);

    ysw_note_t *notes = ysw_heap_allocate(sizeof(ysw_note_t) * note_count);
    for (zm_large_t i = 0; i < note_count; i++) {
        notes[i] = *(ysw_note_t *)ysw_array_get(array, i);
    }

    ysw_array_free(array);

    ysw_seq_message_t message = {
        .type = YSW_SEQ_PLAY,
        .play.notes = notes,
        .play.note_count = note_count,
        .play.tempo = 100,
    };

    ysw_main_seq_send(&message);

    int c;
    while ((c = getchar()) != -1) {
        ESP_LOGD(TAG, "c=%c", c);
    }
}

#endif


