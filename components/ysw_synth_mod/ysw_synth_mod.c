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
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "assert.h"
#include "fcntl.h"
#include "stdlib.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"

#define TAG "YSW_SYNTH_MOD"

#define YSW_MUSIC_SAMPLE YSW_MUSIC_PARTITION "/ST-01/Steinway"

#define MAX_VOICES 32
#define MAX_SAMPLES 32

// Inspired by HxCModPlayer by Jean-François DEL NERO

// Basic type
typedef unsigned char   muchar;
typedef signed   char   mchar;
typedef unsigned short  muint;
typedef          short  mint;
typedef unsigned long   mulong;
typedef unsigned long   mssize;
typedef signed short    msample;

typedef enum {
    PAN_LEFT,
    PAN_CENTER,
    PAN_RIGHT,
} pan_t;

typedef struct {
    mchar  *data;
    muint   length;
    muchar  volume;
    muint   reppnt;
    muint   replen;
    pan_t   pan;
} sample_t;

typedef struct {
    sample_t *sample;
    muint   length;
    muint   reppnt;
    muint   replen;
    mulong  samppos;
    mulong  sampinc;
    muint   period;
    muchar  volume;
} voice_t;

typedef struct {
    sample_t  samples[MAX_SAMPLES];
    mulong  playrate;
    mulong  sampleticksconst;
    voice_t voices[MAX_VOICES];
    muint   number_of_voices;
    muint   mod_loaded;
    mint    last_r_sample;
    mint    last_l_sample;
    mint    stereo;
    mint    stereo_separation;
    mint    filter;
} context_t;

static QueueHandle_t input_queue;
static context_t *context;

static const short period_map[] = {
    /*  0 */ 13696, 12928, 12192, 11520, 10848, 10240,  9664,  9120,  8606,  8128,  7680,  7248,
    /*  1 */  6848,  6464,  6096,  5760,  5424,  5120,  4832,  4560,  4304,  4064,  3840,  3624,
    /*  2 */  3424,  3232,  3048,  2880,  2712,  2560,  2416,  2280,  2152,  2032,  1920,  1812,
    /*  3 */  1712,  1616,  1524,  1440,  1356,  1280,  1208,  1140,  1076,  1016,   960,   906,
    /*  4 */   856,   808,   762,   720,   678,   640,   604,   570,   538,   508,   480,   453,
    /*  5 */   428,   404,   381,   360,   339,   320,   302,   285,   269,   254,   240,   226,
    /*  6 */   214,   202,   190,   180,   170,   160,   151,   143,   135,   127,   120,   113,
    /*  7 */   107,   101,    95,    90,    85,    80,    75,    71,    67,    63,    60,    56,
    /*  8 */    53,    50,    47,    45,    42,    40,    37,    35,    33,    31,    30,    28,
    /*  9 */    27,    25,    24,    22,    21,    20,    19,    18,    17,    16,    15,    14,
    /* 10 */    13,    13,    12,    11,    11,    10,     9,     9,     8,     8,     7,     7
};

#define PERIOD_MAP_SIZE (sizeof(period_map) / sizeof(period_map[0]))

static SemaphoreHandle_t semaphore;

static void enter_critical_section()
{
    xSemaphoreTake(semaphore, portMAX_DELAY);
}

static void leave_critical_section()
{
    xSemaphoreGive(semaphore);
}

static void initialize_synthesizer(context_t *context)
{
    assert(context);

    memset(context, 0, sizeof(context_t));
    context->playrate = 44100;
    context->stereo = 1;
    context->stereo_separation = 1;
    context->filter = 1;
    context->number_of_voices = 0;
    context->sampleticksconst = ((3546894UL * 16) / context->playrate) << 6; //8287*428/playrate;
    context->mod_loaded = 1;

    semaphore = xSemaphoreCreateMutex();
}

void load_sample(context_t *context, const char *name, int index)
{
    assert(index < MAX_SAMPLES);

    struct stat sb;
    int rc = stat(YSW_MUSIC_SAMPLE, &sb);
    if (rc == -1) {
        ESP_LOGE(TAG, "stat failed, file=%s", YSW_MUSIC_SAMPLE);
        abort();
    }

    int sample_size = sb.st_size;

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

    sample_t sample = {
        .data = sample_data,
        .length = sample_size,
        .volume = 60,
        .reppnt = 0,
        .replen = 0,
        .pan = PAN_CENTER,
    };

    enter_critical_section();
    context->samples[index] = sample;
    leave_critical_section();
}

static void fill_buffer(context_t *context, msample *outbuffer, mssize nbsample)
{
    assert(context);
    assert(outbuffer);

    enter_critical_section();

    if (!context->mod_loaded) {
        for (mssize i = 0; i < nbsample; i++)
        {
            *outbuffer++ = 0;
            *outbuffer++ = 0;
        }
        return;
    }

    int last_left = context->last_l_sample;
    int last_right = context->last_r_sample;

    for (mssize i = 0; i < nbsample; i++) {

        int left = 0;
        int right = 0;

        voice_t *voice = context->voices;

        for (muint j = 0; j < context->number_of_voices; j++, voice++) {
            if (voice->period != 0) {
                voice->samppos += voice->sampinc;

                if (voice->replen < 2) {
                    if ((voice->samppos >> 11) >= voice->length) {
                        voice->length = 0;
                        voice->reppnt = 0;
                        voice->samppos = 0;
                    }
                } else {
                    if ((voice->samppos >> 11) >= (unsigned long)(voice->replen + voice->reppnt)) {
                        voice->samppos = ((unsigned long)(voice->reppnt)<<11) + (voice->samppos % ((unsigned long)(voice->replen + voice->reppnt)<<11));
                    }
                }

                unsigned long k = voice->samppos >> 10;

#if 0
                if (voice->samppos) {
                    ESP_LOGD(TAG, "period=%d, sampinc=%d, samppos=%ld, samppos>>11=%d, k=%d, length=%d", voice->period, voice->sampinc, voice->samppos, voice->samppos >> 11, k, voice->length);
                }
#endif

                switch (voice->sample->pan) {
                    case PAN_LEFT:
                        left += (voice->sample->data[k] * voice->volume);
                        break;
                    case PAN_RIGHT:
                        right += (voice->sample->data[k] * voice->volume);
                        break;
                    case PAN_CENTER:
                    default:
                        left += (voice->sample->data[k] * voice->volume);
                        right += (voice->sample->data[k] * voice->volume);
                        break;
                }
            }
        }

        int temp_left = (short)left;
        int temp_right = (short)right;

        if (context->filter) {
            left = (left + last_left) >> 1;
            right = (right + last_right) >> 1;
        }

        if (context->stereo_separation == 1) {
            left = (left + (right >> 1));
            right = (right + (left >> 1));
        }

        if (left > 32767) {
            left = 32767;
        }
        if (left < -32768) {
            left = -32768;
        }
        if (right > 32767) {
            right = 32767;
        }
        if (right < -32768) {
            right = -32768;
        }

        *outbuffer++ = left;
        *outbuffer++ = right;

        last_left = temp_left;
        last_right = temp_right;

    }

    context->last_l_sample = last_left;
    context->last_r_sample = last_right;

    leave_critical_section();
}

static void play_note(context_t *context, sample_t *sample, int period)
{
    enter_critical_section();
    context->number_of_voices = 1;
    voice_t *voice = &context->voices[0];
    voice->sample = sample;
    voice->length = sample->length;
    voice->reppnt = sample->reppnt;
    voice->replen = sample->replen;
    voice->volume = sample->volume;
    voice->sampinc = context->sampleticksconst / period;
    voice->period = period;
    voice->samppos = 0;
    leave_critical_section();
}

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
    fill_buffer(context, (msample*)data, len / 4);
    return len;
}

static void* alsa_thread(void *p)
{
    ysw_alsa_initialize(data_cb);
    return NULL;
}
#endif

static void on_note_on(ysw_synth_note_on_t *m)
{
    if (m->midi_note < PERIOD_MAP_SIZE) {
        zm_medium_t period = period_map[m->midi_note];
        ESP_LOGD(TAG, "channel=%d, midi_note=%d, velocity=%d, period=%d",
                m->channel, m->midi_note, m->velocity, period);
        play_note(context, &context->samples[0], period);
    }
}

static void on_note_off(ysw_synth_note_off_t *m)
{
}

static sample_t *program_samples[YSW_MIDI_MAX_COUNT];

static sample_t *channel_samples[YSW_MIDI_MAX_CHANNELS];

static void on_program_change(ysw_synth_program_change_t *m)
{
    assert(m->channel < YSW_MIDI_MAX_CHANNELS);
    assert(m->program < YSW_MIDI_MAX_COUNT);

    sample_t *sample = program_samples[m->program];
    if (!sample) {
        sample = program_samples[0]; // default sample
    }
    channel_samples[m->channel] = sample;
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

static void run_mod_synth(void *parameters)
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
    ESP_LOGD(TAG, "calling ysw_heap_allocate for context_t, size=%d", sizeof(context_t));
    context = ysw_heap_allocate(sizeof(context_t));

    ESP_LOGD(TAG, "calling initialize_synthesizer, context=%p", context);
    initialize_synthesizer(context);

    load_sample(context, YSW_MUSIC_SAMPLE, 0);

#ifdef IDF_VER
#else
    pthread_t p;
    pthread_create(&p, NULL, &alsa_thread, NULL);
#endif

    ysw_task_create_standard(TAG, run_mod_synth, &input_queue, sizeof(ysw_synth_message_t));
    return input_queue;
}
