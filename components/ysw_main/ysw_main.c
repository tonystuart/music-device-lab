// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// TODO: Refactor the functions if you change the following values

#ifdef IDF_VER
#define ESP32_MAIN 4
#else
#define LINUX_MAIN 4
#endif

#if ESP32_MAIN == 4 || LINUX_MAIN == 4

#include "hxcmod.h"
#include "ysw_heap.h"
#include "ysw_music.h"
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

#define TAG "MAIN4"

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
    lv_obj_t *staff = ysw_staff_create(lv_scr_act(), NULL);
    lv_obj_set_size(staff, 320, 240);
    lv_obj_align(staff, NULL, LV_ALIGN_CENTER, 0, 0);

    ysw_main_synth_initialize();
    ysw_main_seq_initialize();

    zm_music_t *music = zm_read();
    zm_song_t *song = initialize_song(music, 0);
    ysw_array_t *array = zm_render_song(song);
    ysw_staff_set_notes(staff, array);
    play_song(array, song->bpm);
}

#endif // ESP32_MAIN == 4 || LINUX_MAIN == 4

#if ESP32_MAIN == 1

#include "ysw_main_display.h"

#include "ysw_csl.h"
#include "ysw_hpl.h"
#include "ysw_music.h"
#include "ysw_mfr.h"
#include "ysw_main_bus.h"
#include "ysw_main_seq.h"
#include "ysw_spiffs.h"
#include "ysw_style.h"
#include "ysw_main_synth.h"

#include "lvgl/lvgl.h"

#include "esp_log.h"

#define TAG "MAIN1"

static ysw_music_t *music;

static void event_handler(lv_obj_t *btn, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        const char *text = lv_list_get_btn_text(btn);
        if (strcmp(text, "Chords") == 0) {
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

    /*Add buttons to the list*/
    lv_obj_t *list_btn;

    list_btn = lv_list_add_btn(dashboard, LV_SYMBOL_AUDIO, "Chords");
    lv_obj_set_event_cb(list_btn, event_handler);

    list_btn = lv_list_add_btn(dashboard, LV_SYMBOL_BELL, "Progressions");
    lv_obj_set_event_cb(list_btn, event_handler);

    list_btn = lv_list_add_btn(dashboard, LV_SYMBOL_IMAGE, "Globals");
    lv_obj_set_event_cb(list_btn, event_handler);
}

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

void app_main()
{
    ESP_LOGD(TAG, "sizeof(ysw_cs_t)=%d", sizeof(ysw_cs_t));
    esp_log_level_set("spi_master", ESP_LOG_INFO);
    esp_log_level_set("BLEServer", ESP_LOG_INFO);
    esp_log_level_set("BLEDevice", ESP_LOG_INFO);
    esp_log_level_set("BLECharacteristic", ESP_LOG_INFO);
    esp_log_level_set("TRACE_HEAP", ESP_LOG_INFO);
    esp_log_level_set("YSW_HEAP", ESP_LOG_INFO);
    esp_log_level_set("YSW_ARRAY", ESP_LOG_INFO);

    ysw_spiffs_initialize(YSW_MUSIC_PARTITION);
    ysw_main_bus_create();
    ysw_main_display_initialize();
    ysw_main_synth_initialize();
    ysw_main_seq_initialize();
    ysw_style_initialize();

    music = ysw_mfr_read();

    if (music && ysw_music_get_cs_count(music) > 0) {
        create_dashboard();
        ESP_LOGI(TAG, "app_main playing hp loop");
        play_hp_loop(music, 1);
    } else {
        lv_obj_t *mbox1 = lv_msgbox_create(lv_scr_act(), NULL);
        lv_msgbox_set_text(mbox1, "The music partition is empty");
        lv_obj_set_width(mbox1, 200);
        lv_obj_align(mbox1, NULL, LV_ALIGN_CENTER, 0, 0);
    }

    while (1) {
        wait_millis(1);
        lv_task_handler();
    }
}

#elif ESP32_MAIN == 3

#include "ysw_a2dp_source.h"
#include "hxcmod.h"
#include "ysw_heap.h"
#include "ysw_spiffs.h"
#include "ysw_music.h"
#include "esp_log.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "unistd.h"

#define TAG "MAIN3"

static modcontext *modctx;

// NB: len is in bytes (typically 512 when called from ysw_a2dp_source)
static int32_t data_cb(uint8_t *data, int32_t len)
{
    if (len < 0 || data == NULL) {
        return 0;
    }
    hxcmod_fillbuffer(modctx, (msample *)data, len / 4);
    return len;
}

#define YSW_MUSIC_MOD YSW_MUSIC_PARTITION "/music.mod"

void app_main()
{
    ESP_LOGD(TAG, "calling ysw_spiffs_initialize, partition=%s", YSW_MUSIC_PARTITION);
    ysw_spiffs_initialize(YSW_MUSIC_PARTITION);

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

    ysw_a2dp_source_initialize(data_cb);
}

#elif ESP32_MAIN == 4

#include "ysw_spiffs.h"

void app_main()
{
    ESP_LOGD(TAG, "calling ysw_spiffs_initialize, partition=%s", YSW_MUSIC_PARTITION);
    ysw_spiffs_initialize(YSW_MUSIC_PARTITION);

    play_zm_song_on_ysw_synth();
}

#endif


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
#include "lvgl/lvgl.h"
#include "lv_drivers/display/monitor.h"
#include "lv_drivers/indev/mouse.h"
#include "lv_examples/lv_examples.h"
#include "esp_log.h"

#define TAG "MAIN"

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
    ysw_lvgl_hal_init();

    ysw_main_bus_create();
    ysw_main_synth_initialize();
    ysw_main_seq_initialize();
    ysw_style_initialize();

    FILE *music_file = fopen("/spiffs/music.csv", "r");
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

static modcontext *modctx;

// NB: len is in bytes (typically 512 when called from a2dp_source)
static int32_t data_cb(uint8_t *data, int32_t len)
{
    if (len < 0 || data == NULL) {
        return 0;
    }
    hxcmod_fillbuffer(modctx, (msample*)data, len / 4, NULL);
    return len;
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

    ysw_alsa_initialize(data_cb);
}

#elif LINUX_MAIN == 3

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

#define TAG "LINUX_MAIN"

#define YSW_MUSIC_SAMPLE YSW_MUSIC_PARTITION "/ST-01/Steinway"

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

    ESP_LOGD(TAG, "calling stat, file=%s", YSW_MUSIC_SAMPLE);
    struct stat sb;
    int rc = stat(YSW_MUSIC_SAMPLE, &sb);
    if (rc == -1) {
        ESP_LOGE(TAG, "stat failed, file=%s", YSW_MUSIC_SAMPLE);
        abort();
    }

    int sample_size = sb.st_size;

    ESP_LOGD(TAG, "calling ysw_heap_allocate for sample_data, size=%d", sample_size);
    void *sample_data = ysw_heap_allocate(sample_size);

    int fd = open(YSW_MUSIC_SAMPLE, O_RDONLY);
    if (fd == -1) {
        ESP_LOGE(TAG, "open failed, file=%s", YSW_MUSIC_SAMPLE);
        abort();
    }

    rc = read(fd, sample_data, sample_size);
    if (rc != sample_size) {
        ESP_LOGE(TAG, "read failed, rc=%d, sample_size=%d", rc, sample_size);
        abort();
    }

    rc = close(fd);
    if (rc == -1) {
        ESP_LOGE(TAG, "close failed");
        abort();
    }

    sample s = {
        .length = sample_size,
        .finetune = 0,
        .volume = 60,
        .reppnt = 1, // play once
        .replen = 0,
    };

    ysw_copy(s.name, YSW_MUSIC_SAMPLE, sizeof(s.name));

    hxcmod_loadchannel(modctx, &s, sample_data);

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

#elif LINUX_MAIN == 4

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


