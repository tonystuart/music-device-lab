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
#include "ysw_heap.h"
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

static ysw_sequencer_config_t config;
static QueueHandle_t input_queue;

static ysw_sequencer_clip_t active;
static ysw_sequencer_clip_t staged;

static bool loop;
static uint32_t next_note;
static uint32_t start_millis;
static uint8_t playback_speed = YSW_SEQUENCER_SPEED_DEFAULT;

static uint8_t active_count = 0;
static active_note_t active_notes[MAX_POLYPHONY];
static uint8_t programs[YSW_MIDI_MAX_CHANNELS];

static uint32_t tpqn = YSW_TICKS_DEFAULT_TPQN;

static inline time_t t2ms(uint32_t ticks)
{
    return ysw_ticks_to_millis_by_tpqn(ticks, active.tempo, tpqn);
}

static inline bool clip_playing()
{
    return start_millis;
}

static void free_clip(ysw_sequencer_clip_t *clip)
{
    if (clip->notes) {
        ysw_heap_free(clip->notes);
        clip->notes = NULL;
        clip->note_count = 0;
        clip->tempo = 0;
    }
}

static void release_all_notes()
{
    for (uint8_t i = 0; i < active_count; i++) {
        config.on_note_off(active_notes[i].channel, active_notes[i].midi_note);
    }
    active_count = 0;
}

static void play_note(note_t *note, int next_note_index)
{
    if (note->instrument != programs[note->channel]) {
        if (note->channel != YSW_MIDI_DRUM_CHANNEL) {
            config.on_program_change(note->channel, note->instrument);
        }
        programs[note->channel] = note->instrument;
    }

    if (next_note_index != -1) {
        config.on_note_off(note->channel, note->midi_note);
    } else {
        if (active_count < MAX_POLYPHONY) {
            next_note_index = active_count++;
        }
    }

    if (next_note_index != -1) {
        config.on_note_on(note);
        active_notes[next_note_index].channel = note->channel;
        active_notes[next_note_index].midi_note = note->midi_note;
        // Limit duration to avoid issues with VS1053b sustained polyphony
        active_notes[next_note_index].end_time = t2ms(note->start) + min(t2ms(note->duration), 1000);
    } else {
        ESP_LOGE(TAG, "Maximum polyphony exceeded, active_count=%d", active_count);
    }
}

static inline void adjust_playback_start_millis()
{
    uint32_t tick;
    if (next_note == 0) {
        // beginning of clip: start at zero
        tick = 0;
    } else {
        // middle of clip: start at current note
        tick = active.notes[next_note].start;
    }

    uint32_t old_elapsed_millis = t2ms(tick);
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

static void play_clip(ysw_sequencer_clip_t *new_clip)
{
    ESP_LOGD(TAG, "play_clip note_count=%d, tempo=%d", new_clip->note_count, new_clip->tempo);
    if (clip_playing()) {
        ESP_LOGD(TAG, "play_clip releasing notes");
        release_all_notes();
    }
    free_clip(&active);
    active = *new_clip;
    if (active.notes == staged.notes) {
        // playing staged clip: don't free them, just unstage them
        ESP_LOGD(TAG, "play_clip playing staged clip");
        staged.notes = NULL;
        staged.note_count = 0;
        staged.tempo = 0;
    } else {
        // playing new clip: clear any staged clip
        ESP_LOGD(TAG, "play_clip playing new clip");
        free_clip(&staged);
    }
    next_note = 0;
    if (active.note_count) {
        ESP_LOGD(TAG, "play_clip adjusting start millis");
        adjust_playback_start_millis();
    }
    ESP_LOGD(TAG, "play_clip start_millis=%d", start_millis);
}

static void stage_clip(ysw_sequencer_clip_t *new_clip)
{
    ESP_LOGD(TAG, "stage_clip note_count=%d, tempo=%d", new_clip->note_count, new_clip->tempo);
    if (clip_playing()) {
        free_clip(&staged);
        staged = *new_clip;
    } else {
        play_clip(new_clip);
    }
}

static void pause_clip()
{
    ESP_LOGD(TAG, "pause_clip start_millis=%d, next_note=%d, note_count=%d", start_millis, next_note, active.note_count);
    if (clip_playing()) {
        release_all_notes();
    } else {
        // Hitting PAUSE twice is like STOP -- you restart at beginning
        next_note = 0;
    }
    start_millis = 0;
}

static void resume_clip()
{
    ESP_LOGD(TAG, "resume_clip next_note=%d, note_count=%d", next_note, active.note_count);
    if (active.note_count) {
        adjust_playback_start_millis();
    }
}

static void loop_next()
{
    ESP_LOGD(TAG, "loop_next staged.note_count=%d", staged.note_count);
    if (staged.notes) {
        play_clip(&staged);
    } else {
        resume_clip();
    }
}

static void set_tempo(uint8_t new_tempo)
{
    ESP_LOGW(TAG, "set_tempo tempo=%d", new_tempo);
    active.tempo = new_tempo;
}

static void set_loop(bool new_value)
{
    loop = new_value;
}

static void set_playback_speed(uint8_t percent)
{
    playback_speed = percent;
    if (clip_playing()) {
        adjust_playback_start_millis();
    }
}

static void process_message(ysw_sequencer_message_t *message)
{
    switch (message->type) {
        case YSW_SEQUENCER_PLAY:
            play_clip(&message->play);
            break;
        case YSW_SEQUENCER_PAUSE:
            pause_clip();
            break;
        case YSW_SEQUENCER_RESUME:
            resume_clip();
            break;
        case YSW_SEQUENCER_TEMPO:
            set_tempo(message->tempo.qnpm);
            break;
        case YSW_SEQUENCER_LOOP:
            set_loop(message->loop.loop);
            break;
        case YSW_SEQUENCER_STAGE:
            stage_clip(&message->stage);
            break;
        case YSW_SEQUENCER_SPEED:
            set_playback_speed(message->speed.percent);
            break;
        default:
            ESP_LOGW(TAG, "invalid message type=%d", message->type);
            break;
    }
}

static TickType_t process_notes()
{
    TickType_t ticks_to_wait = portMAX_DELAY;
    uint32_t playback_millis = get_current_playback_millis();

    note_t *note;
    if (next_note < active.note_count) {
        note = &active.notes[next_note];
    } else {
        note = NULL;
    }

    int next_note_index = -1;
    active_note_t *next_note_to_end = NULL;

    uint8_t index = 0;
    while (index < active_count) {
        active_note_t *active_note = &active_notes[index];
        if (active_note->end_time < playback_millis) {
            config.on_note_off(active_note->channel, active_note->midi_note);
            if (index + 1 < active_count) {
                // replaced expired note with last one in array
                active_notes[index] = active_notes[active_count - 1];
            }
            // free last member of array
            active_count--;
        } else {
            if (next_note_to_end) {
                if (active_note->end_time < next_note_to_end->end_time) {
                    next_note_to_end = active_note;
                }
            } else {
                next_note_to_end = active_note;
            }
            if (note) {
                if (note->channel == active_note->channel &&
                        // TODO: consider if active_note is transposed by play_note
                        note->midi_note == active_note->midi_note) {
                    next_note_index = index;
                }
            }
            // only increment if we didn't replace an expired note
            index++;
        }
    }

    if (note) {
        uint32_t note_start_time = t2ms(note->start);
        if (note_start_time <= playback_millis) {
            play_note(note, next_note_index);
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
        } else if (loop) {
            if (config.on_state_change) {
                config.on_state_change(YSW_SEQUENCER_STATE_LOOP_COMPLETE);
            }
            next_note = 0;
            loop_next();
            ticks_to_wait = 0;
        } else if (staged.notes) {
            ESP_LOGD(TAG, "playback complete, playing staged clip");
            play_clip(&staged);
            ticks_to_wait = 0;
        } else {
            next_note = 0;
            start_millis = 0;
            ESP_LOGD(TAG, "playback complete, nothing more to do");
            if (config.on_state_change) {
                config.on_state_change(YSW_SEQUENCER_STATE_PLAYBACK_COMPLETE);
            }
        }
    }

    return ticks_to_wait;
}

static void run_sequencer(void* parameters)
{
    ESP_LOGD(TAG, "run_sequencer core=%d", xPortGetCoreID());
    for (;;) {
        TickType_t ticks_to_wait = portMAX_DELAY;
        if (clip_playing()) {
            ticks_to_wait = process_notes();
        }
        if (ticks_to_wait == portMAX_DELAY) {
            if (config.on_state_change) {
                ESP_LOGD(TAG, "run_sequencer NOT_PLAYING");
                config.on_state_change(YSW_SEQUENCER_STATE_NOT_PLAYING);
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
