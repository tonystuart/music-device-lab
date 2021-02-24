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
#include "math.h"
#include "stdlib.h"

#define TAG "YSW_MOD_SYNTH"

#define SAMPLE_RATE 44100.0

#define POS_SCALE_FACTOR 10
#define AMP_SCALE_FACTOR 20

#define CENTS_TO_MILLIS(timecents) (pow(2.0, (timecents)/1200.0))
#define NOTE_TO_FREQUENCY(note) (440.0 * pow(2.0, ((note) - 69) / 12.0))

// TODO: establish naming conventions for scaled values, positions vs indices, sample properties, etc.

// TODO: evaluate cost of generating samples via ysw_bus (or alternative) and remove mutex

static void enter_critical_section(ysw_mod_synth_t *mod_synth)
{
    xSemaphoreTake(mod_synth->mutex, portMAX_DELAY);
}

static void leave_critical_section(ysw_mod_synth_t *mod_synth)
{
    xSemaphoreGive(mod_synth->mutex);
}

static void initialize_synthesizer(ysw_mod_synth_t *mod_synth)
{
    assert(mod_synth);

    mod_synth->playrate = SAMPLE_RATE; // TODO: update with actual clock rate from I2S layer
    mod_synth->stereo = 1;
    mod_synth->stereo_separation = 1;
    mod_synth->filter = 1;
    mod_synth->mod_loaded = 1;
    mod_synth->mutex = xSemaphoreCreateMutex();
}

static int32_t get_ramp_step(voice_t *voice, uint32_t from_pct, uint32_t to_pct)
{
    // TODO: use Christian Schoenebeck’s Fast Exponential Envelope Generator
    //return ((to_pct - from_pct) << AMP_SCALE_FACTOR) / (voice->next_change - voice->iterations);
    int32_t n = to_pct - from_pct;
    int32_t scaled_n = n << AMP_SCALE_FACTOR;
    int32_t d = voice->next_change - voice->iterations;
    int32_t r = scaled_n / d;
    return r;
}

static void apply_amplitude_envelope(ysw_mod_synth_t *mod_synth, voice_t *voice)
{
    switch (voice->state) {
        case YSW_MOD_NOTE_ON:
            voice->iterations = 0;
            voice->amplitude = 0;
            voice->ramp_step = 0;
            voice->next_change = voice->iterations + voice->sample->delay;
            voice->state = YSW_MOD_DELAY;
            // fall through
        case YSW_MOD_DELAY:
            if (voice->iterations >= voice->next_change) {
                voice->state = YSW_MOD_ATTACK;
                voice->next_change = voice->iterations + voice->sample->attack;
                voice->ramp_step = get_ramp_step(voice, 0, mod_synth->percent_gain);
                ESP_LOGD(TAG, "attack iterations=%d, next_change=%d, delta=%d (%g)", voice->iterations, voice->next_change, voice->next_change - voice->iterations, (float)(voice->next_change - voice->iterations)/SAMPLE_RATE);
            }
            voice->amplitude += voice->ramp_step;
            break;
        case YSW_MOD_ATTACK:
            if (voice->iterations >= voice->next_change) {
                voice->state = YSW_MOD_HOLD;
                voice->next_change = voice->iterations + voice->sample->hold;
                voice->ramp_step = 0;
                ESP_LOGD(TAG, "hold iterations=%d, next_change=%d, delta=%d (%g)", voice->iterations, voice->next_change, voice->next_change - voice->iterations, (float)(voice->next_change - voice->iterations)/SAMPLE_RATE);
            }
            voice->amplitude += voice->ramp_step;
            break;
        case YSW_MOD_HOLD:
            if (voice->iterations >= voice->next_change) {
                voice->state = YSW_MOD_DECAY;
                voice->next_change = voice->iterations + voice->sample->decay;
                voice->ramp_step = get_ramp_step(voice, mod_synth->percent_gain, voice->sample->sustain);
                ESP_LOGD(TAG, "decay iterations=%d, next_change=%d, delta=%d (%g), amplitude=%d, ramp_step=%d", voice->iterations, voice->next_change, voice->next_change - voice->iterations, (float)(voice->next_change - voice->iterations)/SAMPLE_RATE, voice->amplitude, voice->ramp_step);
            }
            voice->amplitude += voice->ramp_step;
            break;
        case YSW_MOD_DECAY:
            if (voice->iterations >= voice->next_change) {
                voice->state = YSW_MOD_SUSTAIN;
                voice->ramp_step = 0;
                ESP_LOGD(TAG, "sustain amplitude=%d, sustain=%d, delta=%d", voice->amplitude, voice->sample->sustain, voice->amplitude - voice->sample->sustain);

            }
            voice->amplitude += voice->ramp_step;
            break;
        case YSW_MOD_SUSTAIN:
            if ((voice->amplitude >> AMP_SCALE_FACTOR) <= 0) {
                voice->state = YSW_MOD_IDLE;
                ESP_LOGD(TAG, "sustain->idle");
            }
            break;
        case YSW_MOD_NOTE_OFF:
            voice->state = YSW_MOD_RELEASE;
            voice->next_change = voice->iterations + voice->sample->release;
            voice->ramp_step = get_ramp_step(voice, voice->amplitude >> AMP_SCALE_FACTOR, 0);
            ESP_LOGD(TAG, "note_off iterations=%d, next_change=%d, delta=%d (%g), amplitude=%d, ramp_step=%d", voice->iterations, voice->next_change, voice->next_change - voice->iterations, (float)(voice->next_change - voice->iterations)/SAMPLE_RATE, voice->amplitude, voice->ramp_step);
            voice->amplitude += voice->ramp_step;
            break;
        case YSW_MOD_RELEASE:
            if (voice->iterations >= voice->next_change) {
                voice->state = YSW_MOD_IDLE;
                voice->ramp_step = 0;
                voice->amplitude = 0;
                ESP_LOGD(TAG, "release->idle");
            }
            voice->amplitude += voice->ramp_step;
            break;
        case YSW_MOD_IDLE:
            ESP_LOGD(TAG, "idle");
            break;
    }
    voice->iterations++;
}

/**
 * Generate two-channel 16-bit signed audio samples. There are four bytes per sample (LLRR).
 * @param caller_context pointer to context returned by ysw_mod_synth_create_task
 * @param buffer to be filled with signed 16 bit audio samples for left and right channels
 * @param number of four byte samples to generate (i.e. sizeof(outbuffer) / 4)
 */

void ysw_mod_generate_samples(ysw_mod_synth_t *mod_synth,
        int16_t *buffer, uint32_t number, ysw_mod_sample_type_t sample_type)
{
    assert(mod_synth);
    assert(buffer);

    enter_critical_section(mod_synth);

    if (!mod_synth->mod_loaded) {
        for (uint32_t i = 0; i < number; i++)
        {
            *buffer++ = 0;
            *buffer++ = 0;
        }
        return;
    }

    int last_left = mod_synth->last_left_sample;
    int last_right = mod_synth->last_right_sample;;

    for (uint32_t i = 0; i < number; i++) {

        int left = 0;
        int right = 0;

        voice_t *voice = mod_synth->voices;

        for (uint16_t j = 0; j < mod_synth->voice_count; j++, voice++) {
            if (voice->state != YSW_MOD_IDLE) {

                apply_amplitude_envelope(mod_synth, voice);

                voice->samppos += voice->sampinc;
                uint32_t index = voice->samppos >> POS_SCALE_FACTOR;

                if (voice->replen) {
                    if (index >= (voice->reppnt + voice->replen)) {
                        index = voice->reppnt;
                        voice->samppos = index << POS_SCALE_FACTOR;
                    }
                } else {
                    if (index >= voice->length) {
                        voice->state = YSW_MOD_IDLE;
                    }
                }

                switch (voice->sample->pan) {
                    case YSW_MOD_PAN_LEFT:
                        left += (voice->sample->data[index] * voice->volume);
                        break;
                    case YSW_MOD_PAN_RIGHT:
                        right += (voice->sample->data[index] * voice->volume);
                        break;
                    case YSW_MOD_PAN_CENTER:
                    default:
                        left += (voice->sample->data[index] * voice->volume);
                        right += (voice->sample->data[index] * voice->volume);
                        break;
                }

                left = (left * (voice->amplitude >> AMP_SCALE_FACTOR)) / 100;
                right = (right * (voice->amplitude >> AMP_SCALE_FACTOR)) / 100;
                //ESP_LOGD(TAG, "left=%d, right=%d", left, right);

            }
        }

        int temp_left = (short)left;
        int temp_right = (short)right;

        if (mod_synth->filter) {
            left = (left + last_left) >> 1;
            right = (right + last_right) >> 1;
        }

        if (mod_synth->stereo_separation == 1) {
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

    mod_synth->last_left_sample = last_left;
    mod_synth->last_right_sample = last_right;

    leave_critical_section(mod_synth);
}

static uint8_t allocate_voice(ysw_mod_synth_t *mod_synth, uint8_t channel, uint8_t midi_note)
{
    voice_t *voice = NULL;
    uint8_t voice_index = 0;
    // See if there are any free voices to allocate
    if (mod_synth->voice_count < MAX_VOICES) {
        // If so allocate the first one from the free zone
        voice_index = mod_synth->voice_count++;
        voice = &mod_synth->voices[voice_index];
    } else {
        // Otherwise no free voices to allocate, so iterate through all active voices
        uint32_t time = UINT_MAX;
        for (uint8_t i = 0; i < mod_synth->voice_count; i++) {
            // See if voice has reached end of sample or 100% attenuation
            if (mod_synth->voices[i].state == YSW_MOD_IDLE) {
                ESP_LOGD(TAG, "voice_count=%d, voice[%d] reached end of sample", mod_synth->voice_count, i);
                // Yes, see if it is last one in active zone and will be first one in free zone
                if (--mod_synth->voice_count != voice_index) {
                    // It wasn't last one so replace it with the one that was previously at the end
                    mod_synth->voices[i] = mod_synth->voices[mod_synth->voice_count];
                    // And process the newly relocated voice on next iteration
                    i--;
                }
            } else if (mod_synth->voices[i].time < time) {
                // Otherwise voice is still playing, keep track of oldest voice in case we need to steal it
                time = mod_synth->voices[i].time = time;
                voice_index = i;
            }
        }
        // See if we freed one or more voices
        if (mod_synth->voice_count < MAX_VOICES) {
            // Yes, allocate first one from free zone
            voice_index = mod_synth->voice_count++;
        }
        // At this point, voice_index is either first one in free zone or oldest voice encountered
        voice = &mod_synth->voices[voice_index];
    }
    voice->channel = channel;
    voice->midi_note = midi_note;
    mod_synth->active_notes[channel][midi_note] = voice_index;
    return voice_index;
}

static inline uint32_t scale_timecents(ysw_mod_synth_t *mod_synth, zm_timecents_t timecents)
{
    return CENTS_TO_MILLIS(timecents) * mod_synth->playrate;
}

static inline uint32_t normalize_centibels(ysw_mod_synth_t *mod_synth, zm_centibels_t centibels)
{
    return mod_synth->percent_gain * powf(10.0, (-centibels / 10.0 / 20.0));
}

static void start_note(ysw_mod_synth_t *mod_synth, uint8_t channel, uint8_t midi_note, uint8_t velocity)
{
    // Only one task can load the sample at a time
    enter_critical_section(mod_synth);

    bool is_new = false;
    uint8_t program_index = mod_synth->channel_programs[channel];
    ysw_mod_sample_t *sample = mod_synth->mod_host->provide_sample(
            mod_synth->mod_host->context, program_index, midi_note, &is_new);

    if (is_new) {
        sample->delay = scale_timecents(mod_synth, sample->delay);
        sample->attack = scale_timecents(mod_synth, sample->attack);
        sample->hold = scale_timecents(mod_synth, sample->hold);
        sample->decay = scale_timecents(mod_synth, sample->decay);
        sample->release = scale_timecents(mod_synth, sample->release);
        sample->sustain = normalize_centibels(mod_synth, sample->sustain);
    }

    uint32_t sampinc = ((1 << POS_SCALE_FACTOR) *
        CENTS_TO_MILLIS(sample->fine_tune) *
        SAMPLE_RATE /
        NOTE_TO_FREQUENCY(sample->root_key) /
        mod_synth->playrate) *
        NOTE_TO_FREQUENCY(midi_note);

#if 0
    ESP_LOGD(TAG, "root_key=%d, midi_note=%d, sampinc=%d, old_sampinc=%d",
            sample->root_key, midi_note, sampinc, old_sampinc);
#endif

    uint8_t voice_index = allocate_voice(mod_synth, channel, midi_note);
    voice_t *voice = &mod_synth->voices[voice_index];
    voice->sample = sample;
    voice->length = sample->length;
    voice->reppnt = sample->reppnt;
    voice->replen = sample->replen;
    voice->volume = velocity / 2; // mod volume range is 0-63
    voice->sampinc = sampinc;
    voice->samppos = 0;
    voice->time = mod_synth->voice_time++;
    voice->state = YSW_MOD_NOTE_ON;

    leave_critical_section(mod_synth);

#if 0
    ESP_LOGD(TAG, "channel=%d, program=%d, midi_note=%d, velocity=%d, period=%d, voices=%d",
            channel, program_index, midi_note, velocity, period, mod_synth->voice_count);
#endif
}

static void stop_note(ysw_mod_synth_t *mod_synth, uint8_t channel, uint8_t midi_note)
{
    enter_critical_section(mod_synth);

    uint8_t voice_index = mod_synth->active_notes[channel][midi_note];
    voice_t *voice = &mod_synth->voices[voice_index];
    // If channel and note don't match, voice was stolen, nothing more to do
    if (voice->channel == channel && voice->midi_note == midi_note) {
        // See if voice is already idle
        if (voice->state == YSW_MOD_IDLE) {
            // If so, we can free this voice, see if it's the last allocate voice
            if (--mod_synth->voice_count != voice_index) {
                // If not move former last item to position of newly freed item
                *voice = mod_synth->voices[mod_synth->voice_count];
                mod_synth->active_notes[voice->channel][voice->midi_note] = voice_index;
            }
        } else {
            // Note is still playing, let it end properly, voice will be freed in allocate_voice
            voice->state = YSW_MOD_NOTE_OFF;
        }
    }

    leave_critical_section(mod_synth);
}

static void on_note_on(ysw_mod_synth_t *mod_synth, ysw_event_note_on_t *m)
{
    assert(m->channel < YSW_MIDI_MAX_CHANNELS);
    assert(m->midi_note < YSW_MIDI_MAX_COUNT);
    assert(m->velocity < YSW_MIDI_MAX_COUNT);

    start_note(mod_synth, m->channel, m->midi_note, m->velocity);
}

static void on_note_off(ysw_mod_synth_t *mod_synth, ysw_event_note_off_t *m)
{
    assert(m->channel < YSW_MIDI_MAX_CHANNELS);
    assert(m->midi_note < YSW_MIDI_MAX_COUNT);

    stop_note(mod_synth, m->channel, m->midi_note);
}

static void on_program_change(ysw_mod_synth_t *mod_synth, ysw_event_program_change_t *m)
{
    assert(m->channel < YSW_MIDI_MAX_CHANNELS);
    assert(m->program < YSW_MIDI_MAX_COUNT);

    mod_synth->channel_programs[m->channel] = m->program;
}

static void on_synth_gain(ysw_mod_synth_t *mod_synth, ysw_event_synth_gain_t *m)
{
    mod_synth->percent_gain = m->percent_gain;
}

static void process_event(void *context, ysw_event_t *event)
{
    ysw_mod_synth_t *mod_synth = context;
    switch (event->header.type) {
        case YSW_EVENT_NOTE_ON:
            on_note_on(mod_synth, &event->note_on);
            break;
        case YSW_EVENT_NOTE_OFF:
            on_note_off(mod_synth, &event->note_off);
            break;
        case YSW_EVENT_PROGRAM_CHANGE:
            on_program_change(mod_synth, &event->program_change);
            break;
        case YSW_EVENT_SYNTH_GAIN:
            on_synth_gain(mod_synth, &event->synth_gain);
            break;
        default:
            break;
    }
}

ysw_mod_synth_t *ysw_mod_synth_create_task(ysw_bus_t *bus, ysw_mod_host_t *mod_host)
{
    ysw_mod_synth_t *mod_synth = ysw_heap_allocate(sizeof(ysw_mod_synth_t));
    mod_synth->mod_host = mod_host;
    mod_synth->percent_gain = 100;

    initialize_synthesizer(mod_synth);

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.event_handler = process_event;
    config.context = mod_synth;

    ysw_task_t *task = ysw_task_create(&config);

    ysw_task_subscribe(task, YSW_ORIGIN_EDITOR);
    ysw_task_subscribe(task, YSW_ORIGIN_SEQUENCER);
    ysw_task_subscribe(task, YSW_ORIGIN_SAMPLER);

    return mod_synth;
}
