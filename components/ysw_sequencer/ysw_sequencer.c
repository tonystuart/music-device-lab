// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_sequencer.h"

#include "esp_log.h"
#include "ysw_task.h"
#include "ysw_midi.h"
#include "ysw_ticks.h"

#define TAG "YSW_SEQUENCER"

#define MAX_POLYPHONY 64

typedef struct {
    uint8_t channel;
    uint8_t midi_note;
    time_t end_time;
} active_note_t;

typedef bool (*visitor_t)(active_note_t *active_note, int32_t context);

static ysw_sequencer_config_t config;
static QueueHandle_t input_queue;

static note_t *notes;
static uint8_t playback_speed = YSW_SEQUENCER_SPEED_DEFAULT;
static int16_t loop_count;
static uint32_t note_count;
static uint32_t next_note;
static uint32_t start_millis;

static active_note_t *next_note_to_end;
static active_note_t active_notes[MAX_POLYPHONY];
static uint8_t active_count = 0;
static uint8_t programs[16];

static uint8_t bpm = YSW_TICKS_DEFAULT_BPM;
static uint32_t tpb = YSW_TICKS_DEFAULT_TPB;

static inline time_t t2ms(uint32_t ticks)
{
    return ysw_ticks_to_millis_by_tpb(ticks, bpm, tpb);
}

static void change(uint8_t channel, uint8_t program)
{
    ESP_LOGD(TAG, "program change channel=%d, old=%d, new=%d", channel, programs[channel], program);
    if (channel != YSW_MIDI_DRUM_CHANNEL) {
        config.on_program_change(channel, program);
    }
}

static bool time_based_visitor(active_note_t *active_note, int32_t current_millis)
{
    bool is_expired = active_note->end_time < (uint32_t)current_millis;
    if (!is_expired) {
        if (next_note_to_end) {
            if (active_note->end_time < next_note_to_end->end_time) {
                next_note_to_end = active_note;
            }
        } else {
            next_note_to_end = active_note;
        }
    }
    return is_expired;
}

static bool channel_based_visitor(active_note_t *active_note, int32_t channel)
{
    return active_note->channel == (uint32_t)channel;
}

static bool all_note_visitor(active_note_t *active_note, int32_t context)
{
    return true;
}

static void release_notes(visitor_t visitor, int32_t context)
{
    uint8_t index = 0;
    while (index < active_count) {
        active_note_t *active_note = active_notes + index;
        if ((*visitor)(active_note, context)) {
            config.on_note_off(active_note->channel, active_note->midi_note);
            if (index + 1 < active_count) {
                active_notes[index] = active_notes[active_count-1];
            }
            active_count--;
        } else {
            index++;
        }
    }
}

static void play_note(note_t *note)
{
    if (note->instrument != programs[note->channel]) {
        release_notes(channel_based_visitor, note->channel);
        change(note->channel, note->instrument);
        programs[note->channel] = note->instrument;
    }
    if (active_count < MAX_POLYPHONY) {
        uint8_t active_note;
        if (note->channel == YSW_MIDI_DRUM_CHANNEL) {
            active_note = note->midi_note;
        } else {
            active_note = ysw_song_transpose(note->midi_note);
        }
        config.on_note_on(note->channel, active_note, note->velocity);
        active_notes[active_count].channel = note->channel;
        active_notes[active_count].midi_note = active_note;
        // Limit duration to avoid issues with VS1053b sustained polyphony
        active_notes[active_count].end_time = t2ms(note->start) + min(t2ms(note->duration), 1000);
        active_count++;
    } else {
        ESP_LOGE(TAG, "Maximum polyphony exceeded, active_count=%d", active_count);
    }
}

static inline void adjust_playback_start_millis()
{
    uint32_t old_elapsed_millis = t2ms(notes[next_note].start);
    uint32_t new_elapsed_millis = (100 * old_elapsed_millis) / playback_speed;
    start_millis = get_millis() - new_elapsed_millis;
}

static uint32_t get_current_playback_millis()
{
    uint32_t current_millis = get_millis();
    uint32_t elapsed_millis = current_millis - start_millis;
    uint32_t playback_millis = (elapsed_millis * playback_speed) / 100;
    return playback_millis;
}

static void select_part(uint8_t part_index)
{
    ESP_LOGD(TAG, "select_part part_index=%d", part_index);
}

static void initialize(ysw_sequencer_initialize_t *message)
{
    ESP_LOGD(TAG, "initialize notes=%p, note_count=%d", message->notes, message->note_count);
    if (start_millis) {
        release_notes(all_note_visitor, 0);
    }
    notes = message->notes;
    note_count = message->note_count;
    next_note = 0;
    select_part(0);
    if (start_millis) {
        adjust_playback_start_millis();
    }
}

static void set_tempo(uint8_t percent)
{
    playback_speed = percent;
    if (start_millis) {
        adjust_playback_start_millis();
    }
}

static void play_song()
{
    ESP_LOGD(TAG, "play_song next_note=%d, note_count=%d", next_note, note_count);
    if (note_count) {
        adjust_playback_start_millis();
    }
}

static void process_play(ysw_sequencer_play_t *message)
{
    ESP_LOGD(TAG, "process_play loop_count=%d", message->loop_count);
    loop_count = message->loop_count;
    play_song();
}

static void pause_song()
{
    ESP_LOGD(TAG, "pause_song start_millis=%d, next_note=%d, note_count=%d", start_millis, next_note, note_count);
    if (start_millis) {
        release_notes(all_note_visitor, 0);
    } else {
        // Hitting PAUSE twice is like STOP -- you restart at beginning
        next_note = 0;
    }
    start_millis = 0;
}

static void process_message(ysw_sequencer_message_t *message)
{
    switch (message->type) {
        case YSW_SEQUENCER_INITIALIZE:
            initialize(&message->initialize);
            break;
        case YSW_SEQUENCER_PLAY:
            process_play(&message->play);
            break;
        case YSW_SEQUENCER_PAUSE:
            pause_song();
            break;
        case YSW_SEQUENCER_SET_TEMPO:
            set_tempo(message->set_tempo.percent);
            break;
        default:
            break;
    }
}

static void run_sequencer(void* parameters)
{
    ESP_LOGD(TAG, "run_sequencer core=%d", xPortGetCoreID());
    for (;;) {
        TickType_t ticks_to_wait = portMAX_DELAY;
        if (start_millis) {
            next_note_to_end = NULL;
            uint32_t playback_millis = get_current_playback_millis();
            release_notes(time_based_visitor, playback_millis);
            if (next_note < note_count) {
                note_t *note = &notes[next_note];
                uint32_t note_start_time = t2ms(note->start);
                if (note_start_time <= playback_millis) {
                    note_t *note = &notes[next_note];
                    play_note(note);
                    next_note++;
                    ticks_to_wait = 0;
                } else {
                    long time_of_next_event;
                    if (next_note_to_end && (next_note_to_end->end_time < note_start_time)) {
                        time_of_next_event = next_note_to_end->end_time;
                    } else {
                        time_of_next_event = note_start_time;
                    }
                    uint32_t delay_millis = time_of_next_event - playback_millis;
                    ticks_to_wait = to_ticks(delay_millis);
                }
            } else {
                if (next_note_to_end) {
                    ESP_LOGD(TAG, "waiting for final notes to end");
                    uint32_t delay_millis = next_note_to_end->end_time - playback_millis;
                    ticks_to_wait = to_ticks(delay_millis);
                } else if (loop_count) {
                    if (loop_count != YSW_SEQUENCER_LOOP_REPEATEDLY) {
                        loop_count--;
                    }
                    ESP_LOGD(TAG, "loop_count=%d", loop_count);
                    if (config.on_state_change) {
                        config.on_state_change(YSW_SEQUENCER_STATE_LOOP_COMPLETE);
                    }
                    next_note = 0;
                    play_song();
                    ticks_to_wait = 0;
                } else {
                    next_note = 0;
                    start_millis = 0;
                    ESP_LOGD(TAG, "playback of notes is complete");
                    if (config.on_state_change) {
                        config.on_state_change(YSW_SEQUENCER_STATE_PLAYBACK_COMPLETE);
                    }
                }
            }
        }
        ysw_sequencer_message_t message;
        BaseType_t is_message = xQueueReceive(input_queue, &message, ticks_to_wait);
        if (is_message) {
            process_message(&message);
        }
    }
}

QueueHandle_t ysw_sequencer_create_task(ysw_sequencer_config_t *new_config)
{
    config = *new_config;
    ysw_task_create_standard(TAG, run_sequencer, &input_queue, sizeof(ysw_sequencer_message_t));
    return input_queue;
}
