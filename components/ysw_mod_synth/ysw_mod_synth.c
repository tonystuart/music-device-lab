// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// Inspired by Jean-François del Nero's public domain hxcmod.c

#include "ysw_mod_synth.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_task.h"
#include "esp_log.h"
#include "assert.h"
#include "limits.h"
#include "stdlib.h"

#define TAG "YSW_MOD_SYNTH"

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

static void enter_critical_section(ysw_mod_synth_t *ysw_mod_synth)
{
    xSemaphoreTake(ysw_mod_synth->mutex, portMAX_DELAY);
}

static void leave_critical_section(ysw_mod_synth_t *ysw_mod_synth)
{
    xSemaphoreGive(ysw_mod_synth->mutex);
}

static void initialize_synthesizer(ysw_mod_synth_t *ysw_mod_synth)
{
    assert(ysw_mod_synth);

    ysw_mod_synth->playrate = 44100;
    ysw_mod_synth->stereo = 1;
    ysw_mod_synth->stereo_separation = 1;
    ysw_mod_synth->filter = 1;
    ysw_mod_synth->sampleticksconst = ((3546894UL * 16) / ysw_mod_synth->playrate) << 6; //8287*428/playrate;
    ysw_mod_synth->mod_loaded = 1;
    ysw_mod_synth->mutex = xSemaphoreCreateMutex();
}

/**
 * Generate two-channel 16-bit signed audio samples. There are four bytes per sample (LLRR).
 * @param caller_context pointer to context returned by ysw_mod_synth_create_task
 * @param buffer to be filled with signed 16 bit audio samples for left and right channels
 * @param number of four byte samples to generate (i.e. sizeof(outbuffer) / 4)
 */

void ysw_mod_generate_samples(ysw_mod_synth_t *ysw_mod_synth,
        int16_t *buffer, uint32_t number, ysw_mod_sample_type_t sample_type)
{
    assert(ysw_mod_synth);
    assert(buffer);

    enter_critical_section(ysw_mod_synth);

    if (!ysw_mod_synth->mod_loaded) {
        for (uint32_t i = 0; i < number; i++)
        {
            *buffer++ = 0;
            *buffer++ = 0;
        }
        return;
    }

    int last_left = ysw_mod_synth->last_left_sample;
    int last_right = ysw_mod_synth->last_right_sample;;

    for (uint32_t i = 0; i < number; i++) {

        int left = 0;
        int right = 0;

        voice_t *voice = ysw_mod_synth->voices;

        for (uint16_t j = 0; j < ysw_mod_synth->voice_count; j++, voice++) {
            if (voice->period != 0) {
                voice->samppos += voice->sampinc;

                if (voice->replen < 2) {
                    if ((voice->samppos >> 11) >= voice->length) {
                        voice->length = 0;
                        voice->reppnt = 0;
                        voice->samppos = 0;
                    }
                } else {
                    if ((voice->samppos >> 11) >= (uint32_t)(voice->replen + voice->reppnt)) {
                        voice->samppos = ((uint32_t)(voice->reppnt)<<11) + (voice->samppos % ((uint32_t)(voice->replen + voice->reppnt)<<11));
                    }
                }

                uint32_t k = voice->samppos >> 10;

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

        if (ysw_mod_synth->filter) {
            left = (left + last_left) >> 1;
            right = (right + last_right) >> 1;
        }

        if (ysw_mod_synth->stereo_separation == 1) {
            left = (left + (right >> 1));
            right = (right + (left >> 1));
        }

        if (left > 32767) {
            left = 32767;
        } else if (left < -32768) {
            left = -32768;
        }

        if (right > 32767) {
            right = 32767;
        } else if (right < -32768) {
            right = -32768;
        }

        if (sample_type == YSW_MOD_16BIT_UNSIGNED) {
            left += 32768;
            right += 32768;
        }

        *buffer++ = left;
        *buffer++ = right;

        last_left = temp_left;
        last_right = temp_right;

    }

    ysw_mod_synth->last_left_sample = last_left;
    ysw_mod_synth->last_right_sample = last_right;

    leave_critical_section(ysw_mod_synth);
}

static uint8_t allocate_voice(ysw_mod_synth_t *ysw_mod_synth, uint8_t channel, uint8_t midi_note)
{
    voice_t *voice = NULL;
    uint8_t voice_index = 0;
    // TODO: use stealing logic to free any voices that have reached end of sample
    if (ysw_mod_synth->voice_count < MAX_VOICES) {
        voice_index = ysw_mod_synth->voice_count++;
        voice = &ysw_mod_synth->voices[voice_index];
    } else {
        // steal oldest voice
        uint32_t time = UINT_MAX;
        for (uint8_t i = 0; i < ysw_mod_synth->voice_count; i++) {
            if (ysw_mod_synth->voices[i].time < time) {
                time = ysw_mod_synth->voices[i].time = time;
                voice_index = i;
            }
        }
        voice = &ysw_mod_synth->voices[voice_index];
    }
    voice->channel = channel;
    voice->midi_note = midi_note;
    ysw_mod_synth->active_notes[channel][midi_note] = voice_index;
    return voice_index;
}

static void free_voice(ysw_mod_synth_t *ysw_mod_synth, uint8_t channel, uint8_t midi_note)
{
    uint8_t voice_index = ysw_mod_synth->active_notes[channel][midi_note];
    voice_t *voice = &ysw_mod_synth->voices[voice_index];
    // if channel and note don't match, voice was stolen, nothing more to do
    if (voice->channel == channel && voice->midi_note == midi_note) {
        // if we're not freeing the last voice
        if (--ysw_mod_synth->voice_count != voice_index) {
            // move former last item to position of newly freed item
            *voice = ysw_mod_synth->voices[ysw_mod_synth->voice_count];
            ysw_mod_synth->active_notes[voice->channel][voice->midi_note] = voice_index;
        }
    }
}

static void start_note(ysw_mod_synth_t *ysw_mod_synth, uint8_t channel, uint8_t midi_note, uint8_t velocity)
{
    uint16_t period = period_map[midi_note];
    uint8_t program_index = ysw_mod_synth->channel_programs[channel];
    ysw_mod_sample_t *sample = ysw_mod_synth->mod_host->provide_sample(
            ysw_mod_synth->mod_host->context, program_index, midi_note);

    enter_critical_section(ysw_mod_synth);

    uint8_t voice_index = allocate_voice(ysw_mod_synth, channel, midi_note);
    voice_t *voice = &ysw_mod_synth->voices[voice_index];
    voice->sample = sample;
    voice->length = sample->length;
    voice->reppnt = sample->reppnt;
    voice->replen = sample->replen;
    voice->volume = velocity / 2; // mod volume range is 0-63
    voice->sampinc = ysw_mod_synth->sampleticksconst / period;
    voice->period = period;
    voice->samppos = 0;
    voice->time = ysw_mod_synth->voice_time++;

    leave_critical_section(ysw_mod_synth);

    ESP_LOGD(TAG, "channel=%d, program=%d, midi_note=%d, velocity=%d, period=%d, voices=%d",
            channel, program_index, midi_note, velocity, period, ysw_mod_synth->voice_count);
}

static void stop_note(ysw_mod_synth_t *ysw_mod_synth, uint8_t channel, uint8_t midi_note)
{
    enter_critical_section(ysw_mod_synth);

    free_voice(ysw_mod_synth, channel, midi_note);

    leave_critical_section(ysw_mod_synth);
}

static void on_note_on(ysw_mod_synth_t *ysw_mod_synth, ysw_event_note_on_t *m)
{
    assert(m->channel < YSW_MIDI_MAX_CHANNELS);
    assert(m->midi_note < YSW_MIDI_MAX_COUNT);
    assert(m->velocity < YSW_MIDI_MAX_COUNT);

    start_note(ysw_mod_synth, m->channel, m->midi_note, m->velocity);
}

static void on_note_off(ysw_mod_synth_t *ysw_mod_synth, ysw_event_note_off_t *m)
{
    assert(m->channel < YSW_MIDI_MAX_CHANNELS);
    assert(m->midi_note < YSW_MIDI_MAX_COUNT);

    stop_note(ysw_mod_synth, m->channel, m->midi_note);
}

static void on_program_change(ysw_mod_synth_t *ysw_mod_synth, ysw_event_program_change_t *m)
{
    assert(m->channel < YSW_MIDI_MAX_CHANNELS);
    assert(m->program < YSW_MIDI_MAX_COUNT);

    ysw_mod_synth->channel_programs[m->channel] = m->program;
}

static void process_event(void *context, ysw_event_t *event)
{
    ysw_mod_synth_t *ysw_mod_synth = context;
    switch (event->header.type) {
        case YSW_EVENT_NOTE_ON:
            on_note_on(ysw_mod_synth, &event->note_on);
            break;
        case YSW_EVENT_NOTE_OFF:
            on_note_off(ysw_mod_synth, &event->note_off);
            break;
        case YSW_EVENT_PROGRAM_CHANGE:
            on_program_change(ysw_mod_synth, &event->program_change);
            break;
        default:
            break;
    }
}

ysw_mod_synth_t *ysw_mod_synth_create_task(ysw_bus_t *bus, ysw_mod_host_t *mod_host)
{
    ysw_mod_synth_t *ysw_mod_synth = ysw_heap_allocate(sizeof(ysw_mod_synth_t));
    ysw_mod_synth->mod_host = mod_host;

    initialize_synthesizer(ysw_mod_synth);

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.event_handler = process_event;
    config.context = ysw_mod_synth;

    ysw_task_t *task = ysw_task_create(&config);

    ysw_task_subscribe(task, YSW_ORIGIN_EDITOR);
    ysw_task_subscribe(task, YSW_ORIGIN_SEQUENCER);
    ysw_task_subscribe(task, YSW_ORIGIN_SAMPLER);
    ysw_task_subscribe(task, YSW_ORIGIN_SINK);

    return ysw_mod_synth;
}