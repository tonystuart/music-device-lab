// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_synth_mod.h"
#include "ysw_common.h"
#include "ysw_heap.h"
#include "ysw_midi.h"
#include "ysw_music.h"
#include "ysw_task.h"
#include "ysw_synth.h"
#include "zm_music.h"
#include "hxcmod.h"
#include "esp_log.h"
#include "fcntl.h"
#include "stdlib.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"

#define TAG "YSW_SYNTH_MOD"

#define YSW_MUSIC_MOD YSW_MUSIC_PARTITION "/music.mod"

static QueueHandle_t input_queue;
static modcontext *modctx;

#ifdef IDF_VER
#else
#include "ysw_alsa.h"
#include "pthread.h"

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
#endif

static const zm_medium_t period_map[] = {
    1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016,  960,  906,
    856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  453,
    428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
    214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,
    107,  101,   95,   90,   85,   80,   75,   71,   67,   63,   60,   56,
};

#define PERIOD_MAP_SIZE (sizeof(period_map) / sizeof(period_map[0]))

#define PERIOD_MAP_OFFSET 36 // midi_note that matches mod note 0

static inline void on_note_on(ysw_synth_note_on_t *m)
{
    zm_medium_t mod_note = (m->midi_note - PERIOD_MAP_OFFSET) % PERIOD_MAP_SIZE;
    zm_medium_t period = period_map[mod_note];
    ESP_LOGD(TAG, "channel=%d, midi_note=%d, velocity=%d, mod_note=%d, period=%d",
            m->channel, m->midi_note, m->velocity, mod_note, period);
    extern void play_note(modcontext *modctx, int period);
    play_note(modctx, period);
}

static inline void on_note_off(ysw_synth_note_off_t *m)
{
}

static inline void on_program_change(ysw_synth_program_change_t *m)
{
}

static void process_message(ysw_synth_message_t *message)
{
    switch (message->type) {
        case YSW_SYNTH_NOTE_ON:
            on_note_on(&message->note_on);
            break;
        case YSW_SYNTH_NOTE_OFF:
            on_note_off(&message->note_off);
            break;
        case YSW_SYNTH_PROGRAM_CHANGE:
            on_program_change(&message->program_change);
            break;
        default:
            break;
    }
}

static void run_mod_synth(void* parameters)
{
    for (;;) {
        ysw_synth_message_t message;
        BaseType_t is_message = xQueueReceive(input_queue, &message, portMAX_DELAY);
        if (is_message) {
            process_message(&message);
        }
    }
}

QueueHandle_t ysw_synth_mod_create_task()
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

#ifdef IDF_VER
#else
    pthread_t p;
    pthread_create(&p, NULL, &alsa_thread, NULL);
#endif

    ysw_task_create_standard(TAG, run_mod_synth, &input_queue, sizeof(ysw_synth_message_t));
    return input_queue;
}
