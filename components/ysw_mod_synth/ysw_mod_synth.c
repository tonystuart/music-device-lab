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
#include "ysw_csv.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_midi.h"
#include "ysw_task.h"
#include "esp_log.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "assert.h"
#include "fcntl.h"
#include "limits.h"
#include "math.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"

#define TAG "YSW_MOD_SYNTH"

// TODO: establish naming conventions for scaled values, positions vs indices, sample properties, etc.
// TODO: evaluate cost of generating samples via ysw_bus (or alternative) and remove mutex
// TODO: make ysw_mod_generate_samples private and handle via ysw_bus

#define PATH_SIZE 128
#define RECORD_SIZE 128
#define TOKENS_SIZE 64

#define SAMPLE_RATE 44100.0

#define POS_SCALE_FACTOR 10
#define AMP_SCALE_FACTOR 20
#define AMP_MAX_VALUE 100
#define AMP_MAX_SCALED (AMP_MAX_VALUE << AMP_SCALE_FACTOR)

typedef enum {
    YSW_MOD_PRESET = 0,
    YSW_MOD_INSTRUMENT = 1,
    YSW_MOD_SAMPLE = 2,
} ysw_mod_record_t;

static inline float to_millis(int16_t timecents)
{
    return pow(2.0, timecents / 1200.0);
}

static inline float to_frequency(int8_t note)
{
    return 440.0 * pow(2.0, (note - 69) / 12.0);
}

static inline uint32_t apply_amplitude_envelope(int32_t amplitude, int32_t value)
{
    return (value * (amplitude >> AMP_SCALE_FACTOR)) / AMP_MAX_VALUE;
}

static inline uint32_t apply_percent_gain(uint16_t percent_gain, int32_t value)
{
    return (percent_gain * value) / 100;
}

static inline uint32_t parse_centibels(const char *token)
{
    int16_t centibels = atoi(token);
    float amplitude = powf(10.0, (-centibels / 10.0 / 20.0));
    uint32_t scaled_amplitude = amplitude * (AMP_MAX_VALUE << AMP_SCALE_FACTOR);
    return scaled_amplitude;
}

static inline uint32_t parse_timecents(const char *token)
{
    int16_t timecents = token[0] ? atoi(token) : -32768;
    uint32_t samples = to_millis(timecents) * SAMPLE_RATE;
    return samples;
}

static inline uint32_t calculate_sample_increment(ysw_mod_sample_t *sample, uint8_t midi_note)
{
    uint32_t sampinc = (1 << POS_SCALE_FACTOR) *
        to_millis(sample->fine_tune) /
        to_frequency(sample->root_key) *
        to_frequency(midi_note);
    return sampinc;
}

static ysw_mod_sample_t *parse_sample(ysw_csv_t *csv)
{
    ysw_mod_sample_t *sample = ysw_heap_allocate(sizeof(ysw_mod_sample_t));
    sample->name = ysw_heap_strdup(ysw_csv_get_token(csv, 1));
    sample->loop_start = atoi(ysw_csv_get_token(csv, 2));
    sample->loop_end = atoi(ysw_csv_get_token(csv, 3));
    sample->volume = atoi(ysw_csv_get_token(csv, 4));
    sample->pan = atoi(ysw_csv_get_token(csv, 5));
    sample->loop_type = atoi(ysw_csv_get_token(csv, 6));
    sample->attenuation = atoi(ysw_csv_get_token(csv, 7));
    sample->fine_tune = atoi(ysw_csv_get_token(csv, 8));
    sample->root_key = atoi(ysw_csv_get_token(csv, 9));
    sample->delay = parse_timecents(ysw_csv_get_token(csv, 10));
    sample->attack = parse_timecents(ysw_csv_get_token(csv, 11));
    sample->hold = parse_timecents(ysw_csv_get_token(csv, 12));
    sample->decay = parse_timecents(ysw_csv_get_token(csv, 13));
    sample->sustain = parse_centibels(ysw_csv_get_token(csv, 14));
    sample->release = parse_timecents(ysw_csv_get_token(csv, 15));
    sample->from_note = atoi(ysw_csv_get_token(csv, 16));
    sample->to_note = atoi(ysw_csv_get_token(csv, 17));
    sample->from_velocity = atoi(ysw_csv_get_token(csv, 18));
    sample->to_velocity = atoi(ysw_csv_get_token(csv, 19));

    ESP_LOGD(TAG, "%-12s %-12g %-12g %-12g %-12g %-12g",
            sample->name,
            (float)sample->attack / SAMPLE_RATE,
            (float)sample->hold / SAMPLE_RATE,
            (float)sample->decay / SAMPLE_RATE,
            (float)atoi(ysw_csv_get_token(csv, 14)) / 10,
            (float)sample->release / SAMPLE_RATE);

    return sample;
}

static ysw_mod_instrument_t *parse_instrument(ysw_csv_t *csv)
{
    ysw_mod_instrument_t *instrument = ysw_heap_allocate(sizeof(ysw_mod_instrument_t));
    instrument->samples = ysw_array_create(8);

    ESP_LOGD(TAG, "%-12s %-12s %-12s %-12s %-12s %-12s",
            "Sample", "Attack (s)", "Hold (s)", "Decay (s)", "Sustain (dB)", "Release (s)");

    bool done = false;
    uint32_t token_count = 0;
    while (!done && ((token_count = ysw_csv_parse_next_record(csv)))) {
        ysw_mod_record_t type = atoi(ysw_csv_get_token(csv, 0));
        if (type == YSW_MOD_SAMPLE && token_count == 20) {
            ysw_mod_sample_t *sample = parse_sample(csv);
            ysw_array_push(instrument->samples, sample);
        } else {
            ysw_csv_push_back_record(csv);
            done = true;
        }
    }

    return instrument;
}

static ysw_mod_preset_t *parse_preset(ysw_csv_t *csv)
{
    ysw_mod_preset_t *preset = ysw_heap_allocate(sizeof(ysw_mod_preset_t));
    preset->instruments = ysw_array_create(2);

    preset->name = ysw_heap_strdup(ysw_csv_get_token(csv, 1));
    preset->bank = atoi(ysw_csv_get_token(csv, 2));
    preset->num = atoi(ysw_csv_get_token(csv, 3));

    bool done = false;
    uint32_t token_count = 0;
    while (!done && ((token_count = ysw_csv_parse_next_record(csv)))) {
        ysw_mod_record_t type = atoi(ysw_csv_get_token(csv, 0));
        if (type == YSW_MOD_INSTRUMENT && token_count == 2) {
            ysw_mod_instrument_t *instrument = parse_instrument(csv);
            ysw_array_push(preset->instruments, instrument);
        } else {
            ysw_csv_push_back_record(csv);
            done = true;
        }
    }

    return preset;
}

ysw_mod_preset_t *parse_file(FILE *file)
{
    ysw_mod_preset_t *preset = NULL;
    ysw_csv_t *csv = ysw_csv_create(file, RECORD_SIZE, TOKENS_SIZE);

    uint32_t token_count = 0;
    while ((token_count = ysw_csv_parse_next_record(csv))) {
        ysw_mod_record_t type = atoi(ysw_csv_get_token(csv, 0));
        if (type == YSW_MOD_PRESET && token_count == 4) {
            preset = parse_preset(csv);
        } else {
            ESP_LOGW(TAG, "invalid record_count=%d, record type=%d, token_count=%d",
                    ysw_csv_get_record_count(csv), type, token_count);
            for (uint32_t i = 0; i < token_count; i++) {
                ESP_LOGW(TAG, "token[%d]=%s", i, ysw_csv_get_token(csv, i));
            }
        }
    }

    ysw_csv_free(csv);
    return preset;
}

static void load_preset(ysw_mod_synth_t *mod_synth, ysw_mod_bank_t *bank, uint8_t preset)
{
    char buf[PATH_SIZE];
    snprintf(buf, sizeof(buf), "%s/presets/%03d-%03d.csv", mod_synth->folder, bank->num, preset);

    FILE *file = fopen(buf, "r");
    assert(file);

    ysw_mod_preset_t *p = parse_file(file);
    bank->presets[preset] = p;

    fclose(file);
}

static void *load_sample(ysw_mod_synth_t *mod_synth, const char* name, uint16_t *byte_count)
{
    char path[PATH_SIZE];
    snprintf(path, sizeof(path), "%s/samples/%s", mod_synth->folder, name);

    struct stat sb;
    int rc = stat(path, &sb);
    if (rc == -1) {
        ESP_LOGE(TAG, "stat failed, file=%s", path);
        abort();
    }

    int sample_size = sb.st_size;

    void *sample_data = ysw_heap_allocate(sample_size);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        ESP_LOGE(TAG, "open failed, file=%s", path);
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

    ESP_LOGD(TAG, "load_sample: path=%s, sample_size=%d bytes", path, sample_size);

    if (byte_count) {
        *byte_count = sample_size;
    }

    return sample_data;
}

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

    mod_synth->stereo = 1;
    mod_synth->stereo_separation = 1;
    mod_synth->filter = 1;
    mod_synth->mod_loaded = 1;
    mod_synth->mutex = xSemaphoreCreateMutex();
}

// from and to must already be scaled up when passed to this function
static int32_t get_ramp_step(voice_t *voice, uint32_t from, uint32_t to)
{
    // TODO: use Christian Schoenebeck’s Fast Exponential Envelope Generator
    int32_t n = to - from;
    int32_t d = voice->next_change - voice->iterations;
    int32_t r = d ? n / d : 0;
    return r;
}

static void calculate_amplitude_envelope(ysw_mod_synth_t *mod_synth, voice_t *voice)
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
                voice->ramp_step = get_ramp_step(voice, 0, AMP_MAX_SCALED);
                // ESP_LOGD(TAG, "attack samples=%d (%gs), from=%d, to=%d, step=%d", voice->next_change - voice->iterations, (float)(voice->next_change - voice->iterations)/SAMPLE_RATE, 0, AMP_MAX_SCALED, voice->ramp_step);
            }
            voice->amplitude += voice->ramp_step;
            break;
        case YSW_MOD_ATTACK:
            if (voice->iterations >= voice->next_change) {
                voice->state = YSW_MOD_HOLD;
                voice->next_change = voice->iterations + voice->sample->hold;
                voice->ramp_step = 0;
                // ESP_LOGD(TAG, "hold samples=%d (%gs), from=%d, to=%d, step=%d", voice->next_change - voice->iterations, (float)(voice->next_change - voice->iterations)/SAMPLE_RATE, voice->amplitude, voice->amplitude, voice->ramp_step);
            }
            voice->amplitude += voice->ramp_step;
            break;
        case YSW_MOD_HOLD:
            if (voice->iterations >= voice->next_change) {
                voice->state = YSW_MOD_DECAY;
                //voice->next_change = voice->iterations + voice->sample->decay;
                voice->next_change = voice->iterations + (3 * SAMPLE_RATE);
                voice->ramp_step = get_ramp_step(voice, AMP_MAX_SCALED, voice->sample->sustain);
                ESP_LOGD(TAG, "decay samples=%d (%gs), from=%d, to=%d, step=%d", voice->next_change - voice->iterations, (float)(voice->next_change - voice->iterations)/SAMPLE_RATE, AMP_MAX_SCALED, voice->sample->sustain, voice->ramp_step);
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
            // TODO: consider impact of loop_type
            voice->state = YSW_MOD_RELEASE;
            voice->next_change = voice->iterations + voice->sample->release;
            voice->ramp_step = get_ramp_step(voice, voice->amplitude, 0);
            // ESP_LOGD(TAG, "note_off samples=%d (%g), from=%d, to=%d, step=%d", voice->next_change - voice->iterations, (float)(voice->next_change - voice->iterations)/SAMPLE_RATE, voice->amplitude, 0, voice->ramp_step);
            voice->amplitude += voice->ramp_step;
            break;
        case YSW_MOD_RELEASE:
            if (voice->iterations >= voice->next_change || (voice->amplitude >> AMP_SCALE_FACTOR) <= 0) {
                voice->state = YSW_MOD_IDLE;
                voice->ramp_step = 0;
                voice->amplitude = 0;
                // ESP_LOGD(TAG, "release->idle");
            }
            voice->amplitude += voice->ramp_step;
            break;
        case YSW_MOD_IDLE:
            // ESP_LOGD(TAG, "idle");
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

    int32_t last_left = mod_synth->last_left_sample;
    int32_t last_right = mod_synth->last_right_sample;;

    for (uint32_t i = 0; i < number; i++) {

        int32_t left = 0;
        int32_t right = 0;

        voice_t *voice = mod_synth->voices;

        for (uint16_t j = 0; j < mod_synth->voice_count; j++, voice++) {
            if (voice->state != YSW_MOD_IDLE) {

                voice->samppos += voice->sampinc;
                uint32_t index = voice->samppos >> POS_SCALE_FACTOR;

                if (voice->loop_type == YSW_MOD_LOOP_CONTINUOUS || voice->loop_type == YSW_MOD_LOOP_THROUGH) {
                    if (index >= voice->loop_end) {
                        index = voice->loop_start;
                        voice->samppos = index << POS_SCALE_FACTOR;
                        voice->samppos -= voice->sampinc; // samppos is incremented before next use
                    }
                } else {
                    if (index >= voice->length) {
                        voice->state = YSW_MOD_IDLE;
                    }
                }

                int32_t voice_left = 0;
                int32_t voice_right = 0;

                switch (voice->sample->pan) {
                    case YSW_MOD_PAN_LEFT:
                        voice_left = (voice->sample->data[index] * voice->volume);
                        break;
                    case YSW_MOD_PAN_RIGHT:
                        voice_right = (voice->sample->data[index] * voice->volume);
                        break;
                    case YSW_MOD_PAN_CENTER:
                    default:
                        voice_left = (voice->sample->data[index] * voice->volume);
                        voice_right = (voice->sample->data[index] * voice->volume);
                        break;
                }

                calculate_amplitude_envelope(mod_synth, voice);

                voice_left = apply_amplitude_envelope(voice->amplitude, voice_left);
                voice_right = apply_amplitude_envelope(voice->amplitude, voice_right);

                left += voice_left;
                right += voice_right;

            }
        }

        left = apply_percent_gain(mod_synth->percent_gain, left);
        right = apply_percent_gain(mod_synth->percent_gain, right);

        int32_t temp_left = (short)left;
        int32_t temp_right = (short)right;

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
                // Yes, see if it is last one in active zone and will be first one in free zone
                if (--mod_synth->voice_count != voice_index) {
                    // It wasn't last one so replace it with the one that was previously at the end
                    mod_synth->voices[i] = mod_synth->voices[mod_synth->voice_count];
                    // And process the newly relocated voice on next iteration
                    i--;
                }
            } else if (mod_synth->voices[i].time < time) {
                // Otherwise voice is still playing, keep track of oldest voice in case we need to steal it
                time = mod_synth->voices[i].time;
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
    // ESP_LOGD(TAG, "alloc: index=%d, count=%d, max=%d, time=%d, state=%d", voice_index, mod_synth->voice_count, MAX_VOICES, voice->time, voice->state);
    return voice_index;
}

static ysw_mod_bank_t *find_bank(ysw_mod_synth_t *mod_synth, uint8_t num)
{
    for (uint8_t i = 0; i < YSW_MOD_MAX_BANKS; i++) {
        if (mod_synth->banks[i].num == num) {
            return &mod_synth->banks[i];
        }
    }
    return NULL;
}

static ysw_mod_preset_t *realize_preset(ysw_mod_synth_t *mod_synth, uint8_t bank, uint8_t preset)
{
    ysw_mod_bank_t *b = find_bank(mod_synth, bank);
    assert(b);

    if (!b->presets[preset]) {
        load_preset(mod_synth, b, preset);
    }

    return b->presets[preset];
}

static ysw_mod_sample_t *find_sample(ysw_mod_instrument_t *instrument, uint8_t midi_note)
{
    uint8_t sample_count = ysw_array_get_count(instrument->samples);
    for (uint8_t i = 0; i < sample_count; i++) {
        ysw_mod_sample_t *sample = ysw_array_get(instrument->samples, i);
        if (sample->from_note <= midi_note && midi_note <= sample->to_note) {
            return sample;
        }
    }
    return NULL;
}

static void realize_sample_data(ysw_mod_synth_t *mod_synth, ysw_mod_sample_t *sample)
{
    if (sample && !sample->data) {
        hnode_t *node = hash_lookup(mod_synth->sample_map, sample->name);
        if (node) {
            sample->data = hnode_get(node);
        } else {
            sample->data = load_sample(mod_synth, sample->name, &sample->length);
            if (!hash_alloc_insert(mod_synth->sample_map, sample->name, sample->data)) {
                ESP_LOGE(TAG, "hash_alloc_insert failed");
                abort();
            }
        }
    }
}

static ysw_mod_sample_t *get_sample(ysw_mod_synth_t *mod_synth, ysw_mod_instrument_t *instrument, uint8_t midi_note)
{
    ysw_mod_sample_t *sample = find_sample(instrument, midi_note);
    realize_sample_data(mod_synth, sample);
    return sample;
}

static void start_note(ysw_mod_synth_t *mod_synth, uint8_t channel, uint8_t midi_note, uint8_t velocity)
{
    enter_critical_section(mod_synth);

    uint8_t bank = mod_synth->channel_banks[channel];
    uint8_t preset = mod_synth->channel_presets[channel];
    ysw_mod_preset_t *p = realize_preset(mod_synth, bank, preset);
    uint8_t instrument_count = ysw_array_get_count(p->instruments);
    for (uint8_t i = 0; i < instrument_count; i++) {
        ysw_mod_instrument_t *instrument = ysw_array_get(p->instruments, i);
        ysw_mod_sample_t *sample = get_sample(mod_synth, instrument, midi_note);
        if (sample) {
            uint8_t voice_index = allocate_voice(mod_synth, channel, midi_note);
            voice_t *voice = &mod_synth->voices[voice_index];
            voice->sample = sample;
            voice->length = sample->length;
            voice->loop_start = sample->loop_start;
            voice->loop_end = sample->loop_end;
            voice->loop_type = sample->loop_type;
            voice->volume = velocity / 2; // mod volume range is 0-63
            voice->sampinc = calculate_sample_increment(sample, midi_note);
            voice->samppos = 0;
            voice->time = mod_synth->voice_time++;
            voice->state = YSW_MOD_NOTE_ON;
        }
    }

    leave_critical_section(mod_synth);
}

static void stop_note(ysw_mod_synth_t *mod_synth, uint8_t channel, uint8_t midi_note)
{
    enter_critical_section(mod_synth);

    for (uint8_t i = 0; i < mod_synth->voice_count; i++) {
        voice_t *voice = &mod_synth->voices[i];
        // See if this voice is associated with this channel's note
        if (voice->channel == channel && voice->midi_note == midi_note) {
            if (voice->state >= YSW_MOD_DELAY && voice->state <= YSW_MOD_SUSTAIN) {
                // ESP_LOGD(TAG, "stop: initiating release, index=%d, state=%d", i, voice->state);
                // Note will progress through release and be freed in allocate_voice
                voice->state = YSW_MOD_NOTE_OFF;
            }
        }
    }

    leave_critical_section(mod_synth);
}

static void preload_preset(ysw_mod_synth_t *mod_synth, uint8_t bank, uint8_t preset)
{
    ysw_mod_preset_t *p = realize_preset(mod_synth, bank, preset);
    uint8_t instrument_count = ysw_array_get_count(p->instruments);
    for (uint8_t i = 0; i < instrument_count; i++) {
        ysw_mod_instrument_t *instrument = ysw_array_get(p->instruments, i);
        uint8_t sample_count = ysw_array_get_count(instrument->samples);
        for (uint8_t j = 0; j < sample_count; j++) {
            ysw_mod_sample_t *sample = ysw_array_get(instrument->samples, j);
            realize_sample_data(mod_synth, sample);
        }
    }
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

static void on_bank_select(ysw_mod_synth_t *mod_synth, ysw_event_bank_select_t *m)
{
    assert(m->channel < YSW_MIDI_MAX_CHANNELS);

    mod_synth->channel_banks[m->channel] = m->bank;
}

static void on_program_change(ysw_mod_synth_t *mod_synth, ysw_event_program_change_t *m)
{
    assert(m->channel < YSW_MIDI_MAX_CHANNELS);
    assert(m->program < YSW_MIDI_MAX_COUNT);

    mod_synth->channel_presets[m->channel] = m->program;

    if (m->preload) {
        uint8_t bank = mod_synth->channel_banks[m->channel];
        preload_preset(mod_synth, bank, m->program);
    }
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
        case YSW_EVENT_BANK_SELECT:
            on_bank_select(mod_synth, &event->bank_select);
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

ysw_mod_synth_t *ysw_mod_synth_create_task(ysw_bus_t *bus)
{
    extern void hash_ensure_assert_off(void);
    hash_ensure_assert_off();

    ysw_mod_synth_t *mod_synth = ysw_heap_allocate(sizeof(ysw_mod_synth_t));
    mod_synth->percent_gain = 100;
    mod_synth->folder = "/spiffs";
    mod_synth->banks[0].num = 0;
    mod_synth->banks[1].num = YSW_MIDI_DRUM_BANK;
    mod_synth->sample_map = hash_create(128, NULL, NULL);

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
