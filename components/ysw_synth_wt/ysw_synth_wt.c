// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_synth_wt.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/dac.h"
#include "driver/i2s.h"
#include "ysw_common.h"
#include "ysw_midi.h"
#include "ysw_task.h"
#include "ysw_wavetable.h"
#include "ysw_synth.h"

#define TAG "YSW_SYNTH_WT"

#define GAIN_IN_DB (-10.0f)
#define BUFFER_COUNT 16
#define SAMPLE_COUNT 8
#define CHANNEL_COUNT 2

//#define SAMPLE_RATE 22050.0
#define SAMPLE_RATE 44100.0

#define POT 8 // power of two; must match with scale_table values
#define ENVPOT 7
#define OSCILLATOR_COUNT 8 // 16 oscillators: 25% load with -O1 (64: 90%)
#define CLIP 1016 // 127 * 8

#define FOREGROUND_VOLUME 10
#define BACKGROUND_VOLUME 1

static SemaphoreHandle_t synth_semaphore;
static QueueHandle_t input_queue;

static const i2s_config_t i2s_config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = 16,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = 0,
    .dma_buf_count = BUFFER_COUNT,
    .dma_buf_len = SAMPLE_COUNT * CHANNEL_COUNT * sizeof(uint16_t),
    .use_apll = false
};

static void enter_critical_section()
{
    xSemaphoreTake(synth_semaphore, portMAX_DELAY);
}

static void leave_critical_section()
{
    xSemaphoreGive(synth_semaphore);
}

uint16_t increments_pot[ OSCILLATOR_COUNT ];
uint32_t phase_accu_pot[ OSCILLATOR_COUNT ];
uint32_t envelope_positions_envpot[ OSCILLATOR_COUNT ];
uint32_t end_times[OSCILLATOR_COUNT];
uint8_t volume_pot[OSCILLATOR_COUNT];

static void run_synth(void* parameters)
{
    const uint32_t sizeof_wt_pot = ( (uint32_t)sizeof( wt ) << POT );
    const uint32_t sizeof_wt_sustain_pot = ( (uint32_t)sizeof( wt_sustain ) << POT );
    const uint32_t sizeof_wt_attack_pot = ( (uint32_t)sizeof( wt_attack ) << POT );
    const uint32_t sizeof_envelope_table_envpot = ( (uint32_t)sizeof( envelope_table ) << ENVPOT );
    for (;;) { 
        int32_t value = 0;
        enter_critical_section();
        int32_t time = get_millis();
        for ( uint8_t osc = 0; osc < OSCILLATOR_COUNT; ++osc ) {
            if (end_times[osc] != 0 && end_times[osc] <= time) {
                increments_pot[ osc ] = 0;
                phase_accu_pot[ osc ] = 0;
                envelope_positions_envpot[ osc ] = 0;
                end_times[osc] = 0;
            }
            phase_accu_pot[ osc ] += increments_pot[ osc ];
            if ( phase_accu_pot[ osc ] >= sizeof_wt_pot ) {
                phase_accu_pot[ osc ] -= sizeof_wt_sustain_pot;
            }
            uint16_t phase_accu = ( phase_accu_pot[ osc ] >> POT );
            value += envelope_table[ envelope_positions_envpot[ osc ] >> ENVPOT ] * (wt[ phase_accu ] * volume_pot[osc]);
            if ( phase_accu_pot[ osc ] >= sizeof_wt_attack_pot &&
                    envelope_positions_envpot[ osc ] < sizeof_envelope_table_envpot - 1 )
            {
                ++envelope_positions_envpot[ osc ];
            }
        }
        leave_critical_section();
        value >>= 8; // envelope_table resolution
        if ( value > CLIP ) {
            value = CLIP;
        } else if ( value < -CLIP ) {
            value = -CLIP;
        }
        size_t write_count = 0;
        uint16_t unsigned_value = (1024 + value) << 5;
        uint32_t item = (unsigned_value << 16) | unsigned_value;
        i2s_write(I2S_NUM_0, &item, sizeof(item), &write_count, portMAX_DELAY);
    }
}

static void validate_pin_assignment(dac_channel_t channel, uint8_t pin)
{
    gpio_num_t gpio_num = 0;
    esp_err_t rc = dac_pad_get_io_num(channel, &gpio_num);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "validate_pin_assignment dac_pad_get_io_num failed (%d)", rc);
        abort();
    }
    if (gpio_num != pin) {
        ESP_LOGE(TAG, "validate_pin_assignment expected pin %d, got %d", gpio_num, pin);
        abort();
    }
}

void create_synth_task(uint8_t dac_left_gpio, uint8_t dac_right_gpio)
{
    ESP_LOGD(TAG, "create_synth_task: entered");

    for ( uint8_t osc = 0; osc < OSCILLATOR_COUNT; ++osc ) {
        increments_pot[ osc ] = 0;
        phase_accu_pot[ osc ] = 0;
        envelope_positions_envpot[ osc ] = 0;
    }

    esp_err_t esp_rc = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (esp_rc != ESP_OK) {
        ESP_LOGE(TAG, "i2s_driver_install failed");
        abort();
    }

    validate_pin_assignment(DAC_CHANNEL_1, dac_right_gpio);
    validate_pin_assignment(DAC_CHANNEL_2, dac_left_gpio);

    esp_rc = i2s_set_pin(I2S_NUM_0, NULL);
    if (esp_rc != ESP_OK) {
        ESP_LOGE(TAG, "i2s_set_pin failed");
        abort();
    }

    synth_semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(synth_semaphore);

    ysw_task_create_standard(TAG, run_synth, NULL, 0);

    ESP_LOGD(TAG, "create_synth_task: returning");
}

uint8_t channel_notes[16][128];

static void play(uint8_t channel, uint8_t midi_key, uint8_t velocity, uint16_t millis)
{
    enter_critical_section();
    static uint8_t next_osc = 0;
    channel_notes[channel][midi_key] = next_osc;
    increments_pot[ next_osc ] = scale_table[midi_key];
    phase_accu_pot[ next_osc ] = 0;
    envelope_positions_envpot[ next_osc ] = 0;
    if (millis) {
        end_times[next_osc] = get_millis() + millis;
    } else {
        end_times[next_osc] = 0;
    }
    volume_pot[next_osc] = velocity;
    ++next_osc;
    if ( next_osc >= OSCILLATOR_COUNT ) {
        next_osc = 0;
    }
    leave_critical_section();
}

void play_note(uint8_t channel, uint8_t midi_key, uint8_t velocity, uint16_t millis)
{
    if (channel != YSW_MIDI_DRUM_CHANNEL) {
        if (velocity) {
            play(channel, midi_key, BACKGROUND_VOLUME, millis);
        }
    }
}

void release_midi_key(uint8_t channel, uint8_t midi_key)
{
    if (channel != YSW_MIDI_DRUM_CHANNEL) {
        enter_critical_section();
        uint8_t osc = channel_notes[channel][midi_key];
        increments_pot[ osc ] = 0;
        phase_accu_pot[ osc ] = 0;
        envelope_positions_envpot[ osc ] = 0;
        end_times[osc] = 0;
        volume_pot[osc] = 0;
        leave_critical_section();
    }
}

void press_midi_key(uint8_t channel, uint8_t midi_key, uint8_t velocity)
{
    if (channel != YSW_MIDI_DRUM_CHANNEL) {
        if (velocity) {
            ESP_LOGD(TAG, "press_midi_key: channel=%d, midi_key=%d, velocity=%d",
                    channel, midi_key, velocity);
            play(channel, midi_key, FOREGROUND_VOLUME, 0);
        } else {
            release_midi_key(channel, midi_key);
        }
    }
}

static inline void on_note_on(ysw_synth_note_on_t *m)
{
    press_midi_key(m->channel, m->midi_note, m->velocity);
}

static inline void on_note_off(ysw_synth_note_off_t *m)
{
    release_midi_key(m->channel, m->midi_note);
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

static void run_wt_synth(void* parameters)
{
    for (;;) {
        ysw_synth_message_t message;
        BaseType_t is_message = xQueueReceive(input_queue, &message, portMAX_DELAY);
        if (is_message) {
            process_message(&message);
        }
    }
}

QueueHandle_t ysw_synth_wt_create_task(uint8_t dac_left_gpio, uint8_t dac_right_gpio)
{
    create_synth_task(dac_left_gpio, dac_right_gpio);
    ysw_task_create_standard(TAG, run_wt_synth, &input_queue, sizeof(ysw_synth_message_t));
    return input_queue;
}
