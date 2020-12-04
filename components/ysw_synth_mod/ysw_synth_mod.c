// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// Inspired by Jean-François del Nero's public domain hxcmod.c

#include "ysw_synth_mod.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_midi.h"
#include "ysw_task.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "assert.h"
#include "limits.h"
#include "stdlib.h"

// TODO: rename ysw_synth_mod to ysw_mod_synth so that externs begin with ysw_mod

#define TAG "YSW_SYNTH_MOD"

#define MAX_VOICES 32

typedef struct {
    ysw_mod_sample_t *sample;
    uint32_t  samppos;
    uint32_t  sampinc;
    uint32_t  time;
    uint16_t   length;
    uint16_t   reppnt;
    uint16_t   replen;
    uint16_t   period;
    uint8_t  volume;
    uint8_t  channel;
    uint8_t  midi_note;
} voice_t;

typedef struct {
    uint8_t active_notes[YSW_MIDI_MAX_CHANNELS][YSW_MIDI_MAX_COUNT];
    uint8_t channel_programs[YSW_MIDI_MAX_CHANNELS];
    uint32_t  playrate;
    uint32_t  sampleticksconst;
    uint32_t  voice_time;
    voice_t voices[MAX_VOICES];
    uint16_t   voice_count;
    uint16_t   mod_loaded;
    int16_t    last_left_sample;
    int16_t    last_right_sample;;
    int16_t    stereo;
    int16_t    stereo_separation;
    int16_t    filter;
    SemaphoreHandle_t mutex;
    ysw_mod_host_t *mod_host;
} context_t;

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

// TODO: Consider using thread local storage instead of static variable

static context_t *data_cb_context;

static void enter_critical_section(context_t *context)
{
    xSemaphoreTake(context->mutex, portMAX_DELAY);
}

static void leave_critical_section(context_t *context)
{
    xSemaphoreGive(context->mutex);
}

static void initialize_synthesizer(context_t *context)
{
    assert(context);

    context->playrate = 44100;
    context->stereo = 1;
    context->stereo_separation = 1;
    context->filter = 1;
    context->sampleticksconst = ((3546894UL * 16) / context->playrate) << 6; //8287*428/playrate;
    context->mod_loaded = 1;
    context->mutex = xSemaphoreCreateMutex();
}

static void fill_buffer(context_t *context, int16_t *outbuffer, uint32_t nbsample)
{
    assert(context);
    assert(outbuffer);

    enter_critical_section(context);

    if (!context->mod_loaded) {
        for (uint32_t i = 0; i < nbsample; i++)
        {
            *outbuffer++ = 0;
            *outbuffer++ = 0;
        }
        return;
    }

    int last_left = context->last_left_sample;
    int last_right = context->last_right_sample;;

    for (uint32_t i = 0; i < nbsample; i++) {

        int left = 0;
        int right = 0;

        voice_t *voice = context->voices;

        for (uint16_t j = 0; j < context->voice_count; j++, voice++) {
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
                    ESP_LOGD(TAG, "period=%d, sampinc=%d, samppos=%d, samppos>>11=%d, k=%d, length=%d", (int)voice->period, (int)voice->sampinc, (int)voice->samppos, (int)(voice->samppos >> 11), (int)k, (int)voice->length);
                }
#endif

                switch (voice->sample->pan) {
                    case YSW_MOD_PAN_LEFT:
                        left += (voice->sample->data[k] * voice->volume);
                        break;
                    case YSW_MOD_PAN_RIGHT:
                        right += (voice->sample->data[k] * voice->volume);
                        break;
                    case YSW_MOD_PAN_CENTER:
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

    context->last_left_sample = last_left;
    context->last_right_sample = last_right;

    leave_critical_section(context);
}

static uint8_t allocate_voice(context_t *context, uint8_t channel, uint8_t midi_note)
{
    voice_t *voice = NULL;
    uint8_t voice_index = 0;
    // TODO: use stealing logic to free any voices that have reached end of sample
    if (context->voice_count < MAX_VOICES) {
        voice_index = context->voice_count++;
        voice = &context->voices[voice_index];
    } else {
        // steal oldest voice
        uint32_t time = UINT_MAX;
        for (uint8_t i = 0; i < context->voice_count; i++) {
            if (context->voices[i].time < time) {
                time = context->voices[i].time = time;
                voice_index = i;
            }
        }
        voice = &context->voices[voice_index];
    }
    voice->channel = channel;
    voice->midi_note = midi_note;
    context->active_notes[channel][midi_note] = voice_index;
    return voice_index;
}

static void free_voice(context_t *context, uint8_t channel, uint8_t midi_note)
{
    uint8_t voice_index = context->active_notes[channel][midi_note];
    voice_t *voice = &context->voices[voice_index];
    // if channel and note don't match, voice was stolen, nothing more to do
    if (voice->channel == channel && voice->midi_note == midi_note) {
        // if we're not freeing the last voice
        if (--context->voice_count != voice_index) {
            // move former last item to position of newly freed item
            *voice = context->voices[context->voice_count];
            context->active_notes[voice->channel][voice->midi_note] = voice_index;
        }
    }
}

static void start_note(context_t *context, uint8_t channel, uint8_t midi_note, uint8_t velocity)
{
    enter_critical_section(context);

    uint8_t voice_index = allocate_voice(context, channel, midi_note);
    uint8_t program_index = context->channel_programs[channel];

    uint16_t period = period_map[midi_note];
    ysw_mod_sample_t *sample = context->mod_host->provide_sample(
            context->mod_host->host_context, program_index, midi_note);

    ESP_LOGD(TAG, "channel=%d, program=%d, midi_note=%d, velocity=%d, period=%d, voices=%d",
            channel, program_index, midi_note, velocity, period, context->voice_count);

    voice_t *voice = &context->voices[voice_index];
    voice->sample = sample;
    voice->length = sample->length;
    voice->reppnt = sample->reppnt;
    voice->replen = sample->replen;
    voice->volume = sample->volume;
    voice->sampinc = context->sampleticksconst / period;
    voice->period = period;
    voice->samppos = 0;
    voice->time = context->voice_time++;

    leave_critical_section(context);
}

static void stop_note(context_t *context, uint8_t channel, uint8_t midi_note)
{
    enter_critical_section(context);

    free_voice(context, channel, midi_note);

    leave_critical_section(context);
}

static void on_note_on(context_t *context, ysw_event_note_on_t *m)
{
    assert(m->channel < YSW_MIDI_MAX_CHANNELS);
    assert(m->midi_note < YSW_MIDI_MAX_COUNT);
    assert(m->velocity < YSW_MIDI_MAX_COUNT);

    start_note(context, m->channel, m->midi_note, m->velocity);
}

static void on_note_off(context_t *context, ysw_event_note_off_t *m)
{
    assert(m->channel < YSW_MIDI_MAX_CHANNELS);
    assert(m->midi_note < YSW_MIDI_MAX_COUNT);

    stop_note(context, m->channel, m->midi_note);
}

static void on_program_change(context_t *context, ysw_event_program_change_t *m)
{
    assert(m->channel < YSW_MIDI_MAX_CHANNELS);
    assert(m->program < YSW_MIDI_MAX_COUNT);

    context->channel_programs[m->channel] = m->program;
}

static void process_event(void *caller_context, ysw_event_t *event)
{
    context_t *context = caller_context;
    switch (event->header.type) {
        case YSW_EVENT_NOTE_ON:
            on_note_on(context, &event->note_on);
            break;
        case YSW_EVENT_NOTE_OFF:
            on_note_off(context, &event->note_off);
            break;
        case YSW_EVENT_PROGRAM_CHANGE:
            on_program_change(context, &event->program_change);
            break;
        default:
            break;
    }
}

// NB: len is in bytes (typically 512 when called from a2dp_source)
static int32_t data_cb(uint8_t *data, int32_t len)
{
    if (len < 0 || data == NULL) {
        return 0;
    }
    fill_buffer(data_cb_context, (int16_t*)data, len / 4);
    return len;
}

#ifdef IDF_VER

#include "ysw_a2dp_source.h"

static void initialize_audio_source(context_t *context)
{
    ysw_a2dp_source_initialize(data_cb);
}

#else

#include "ysw_alsa.h"
#include "pthread.h"

static void* alsa_thread(void *p)
{
    ysw_alsa_initialize(data_cb);
    return NULL;
}

static void initialize_audio_source(context_t *context)
{
    pthread_t p;
    pthread_create(&p, NULL, &alsa_thread, NULL);
}

#endif

void ysw_synth_mod_create_task(ysw_bus_h bus, ysw_mod_host_t *mod_host)
{
    context_t *context = ysw_heap_allocate(sizeof(context_t));
    data_cb_context = context;
    context->mod_host = mod_host;

    initialize_synthesizer(context);
    initialize_audio_source(context);

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.event_handler = process_event;
    config.caller_context = context;

    ysw_task_h task = ysw_task_create(&config);

    ysw_task_subscribe(task, YSW_ORIGIN_EDITOR);
    ysw_task_subscribe(task, YSW_ORIGIN_SEQUENCER);
    ysw_task_subscribe(task, YSW_ORIGIN_SAMPLER);
    ysw_task_subscribe(task, YSW_ORIGIN_SINK);
}
