// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

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
    hxcmod_fillbuffer(modctx, (msample*)data, len / 4, NULL);
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
    if (!hxcmod_init(modctx)) {
        ESP_LOGE(TAG, "hxcmod_init failed");
        abort();
    }

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

    if (!hxcmod_load(modctx, mod_data, mod_data_size)) {
        ESP_LOGE(TAG, "hxcmod_load failed");
        abort();
    }

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
