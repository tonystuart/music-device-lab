// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_seq.h"

#include "esp_log.h"
#include "ysw_heap.h"
#include "ysw_task.h"
#include "ysw_midi.h"
#include "ysw_ticks.h"

#define TAG "YSW_SEQ"

#define MAX_POLYPHONY 64

typedef struct {
    uint8_t channel;
    uint8_t midi_note;
    time_t end_time;
} active_note_t;

typedef struct {
    bool loop;
    uint32_t next_note;
    uint32_t start_millis;
    uint8_t active_count;
    ysw_seq_clip_t active;
    ysw_seq_clip_t staged;
    ysw_seq_config_t config;
    QueueHandle_t input_queue;
    uint8_t playback_speed;
    uint8_t programs[YSW_MIDI_MAX_CHANNELS];
    active_note_t active_notes[MAX_POLYPHONY];
} ysw_seq_t;

static inline time_t t2ms(ysw_seq_t *seq, uint32_t ticks)
{
    return ysw_ticks_to_millis_by_tpqn(ticks, seq->active.tempo, YSW_TICKS_DEFAULT_TPQN);
}

static inline bool clip_playing(ysw_seq_t *seq)
{
    return seq->start_millis;
}

static void free_clip(ysw_seq_clip_t *clip)
{
    if (clip->notes) {
        ysw_heap_free(clip->notes);
        clip->notes = NULL;
        clip->note_count = 0;
        clip->tempo = 0;
    }
}

static void release_all_notes(ysw_seq_t *seq)
{
    for (uint8_t i = 0; i < seq->active_count; i++) {
        seq->config.on_note_off(seq->active_notes[i].channel, seq->active_notes[i].midi_note);
    }
    seq->active_count = 0;
}

static void fire_loop_done(ysw_seq_t *seq)
{
    if (seq->active.on_status) {
        ysw_seq_status_message_t message = {
            .type = YSW_SEQ_LOOP_DONE,
        };
        seq->active.on_status(seq->active.on_status_context, &message);
    }
}

static void fire_play_done(ysw_seq_t *seq)
{
    if (seq->active.on_status) {
        ysw_seq_status_message_t message = {
            .type = YSW_SEQ_PLAY_DONE,
        };
        seq->active.on_status(seq->active.on_status_context, &message);
    }
}

static void fire_idle(ysw_seq_t *seq)
{
    if (seq->active.on_status) {
        ysw_seq_status_message_t message = {
            .type = YSW_SEQ_IDLE,
        };
        seq->active.on_status(seq->active.on_status_context, &message);
    }
}

static void fire_note_status(ysw_seq_t *seq, ysw_note_t *note)
{
    if (seq->active.on_status) {
        ysw_seq_status_message_t message = {
            .type = YSW_SEQ_NOTE,
            .note = note,
        };
        seq->active.on_status(seq->active.on_status_context, &message);
    }
}

static void play_note(ysw_seq_t *seq, ysw_note_t *note, int next_note_index)
{
    if (note->instrument != seq->programs[note->channel]) {
        if (note->channel != YSW_MIDI_DRUM_CHANNEL) {
            seq->config.on_program_change(note->channel, note->instrument);
        }
        seq->programs[note->channel] = note->instrument;
    }

    if (next_note_index != -1) {
        seq->config.on_note_off(note->channel, note->midi_note);
    } else {
        if (seq->active_count < MAX_POLYPHONY) {
            next_note_index = seq->active_count++;
        }
    }

    if (next_note_index != -1) {
        if (note->channel == YSW_MIDI_STATUS_CHANNEL) {
            fire_note_status(seq, note);
        } else {
            seq->config.on_note_on(note);
        }
        seq->active_notes[next_note_index].channel = note->channel;
        seq->active_notes[next_note_index].midi_note = note->midi_note;
        seq->active_notes[next_note_index].end_time = t2ms(seq, note->start) + t2ms(seq, note->duration);
    } else {
        ESP_LOGE(TAG, "Maximum polyphony exceeded, active_count=%d", seq->active_count);
    }
}

static inline void adjust_playback_start_millis(ysw_seq_t *seq)
{
    uint32_t tick;
    if (seq->next_note == 0) {
        // beginning of clip: start at zero
        tick = 0;
    } else {
        // middle of clip: start at current note
        tick = seq->active.notes[seq->next_note].start;
    }

    uint32_t old_elapsed_millis = t2ms(seq, tick);
    uint32_t new_elapsed_millis = (100 * old_elapsed_millis) / seq->playback_speed;
    seq->start_millis = get_millis() - new_elapsed_millis;
}

static uint32_t get_current_playback_millis(ysw_seq_t *seq)
{
    uint32_t current_millis = get_millis();
    uint32_t elapsed_millis = current_millis - seq->start_millis;
    uint32_t playback_millis = (elapsed_millis * seq->playback_speed) / 100;
    return playback_millis;
}

static void play_clip(ysw_seq_t *seq, ysw_seq_clip_t *new_clip)
{
    ESP_LOGD(TAG, "play_clip note_count=%d, tempo=%d", new_clip->note_count, new_clip->tempo);
    if (clip_playing(seq)) {
        ESP_LOGD(TAG, "play_clip releasing notes");
        release_all_notes(seq);
    }
    free_clip(&seq->active);
    seq->active = *new_clip;
    if (seq->active.notes == seq->staged.notes) {
        // playing staged clip: don't free them, just unstage them
        ESP_LOGD(TAG, "play_clip playing staged clip");
        seq->staged.notes = NULL;
        seq->staged.note_count = 0;
        seq->staged.tempo = 0;
    } else {
        // playing new clip: clear any staged clip
        ESP_LOGD(TAG, "play_clip playing new clip");
        free_clip(&seq->staged);
    }
    seq->next_note = 0;
    if (seq->active.note_count) {
        ESP_LOGD(TAG, "play_clip adjusting start millis");
        adjust_playback_start_millis(seq);
    }
    ESP_LOGD(TAG, "play_clip start_millis=%d", seq->start_millis);
}

static void stage_clip(ysw_seq_t *seq, ysw_seq_clip_t *new_clip)
{
    ESP_LOGD(TAG, "stage_clip note_count=%d, tempo=%d", new_clip->note_count, new_clip->tempo);
    if (clip_playing(seq)) {
        free_clip(&seq->staged);
        seq->staged = *new_clip;
    } else {
        play_clip(seq, new_clip);
    }
}

static void pause_clip(ysw_seq_t *seq)
{
    ESP_LOGD(TAG, "pause_clip start_millis=%d, next_note=%d, note_count=%d", seq->start_millis, seq->next_note, seq->active.note_count);
    if (clip_playing(seq)) {
        release_all_notes(seq);
    } else {
        // Hitting PAUSE twice is like STOP -- you restart at beginning
        seq->next_note = 0;
    }
    seq->start_millis = 0;
}

static void stop_clip(ysw_seq_t *seq)
{
    ESP_LOGD(TAG, "stop_clip start_millis=%d, next_note=%d, note_count=%d", seq->start_millis, seq->next_note, seq->active.note_count);
    if (clip_playing(seq)) {
        release_all_notes(seq);
    }
    seq->next_note = 0;
    seq->start_millis = 0;
    seq->active.on_status = NULL; // callback may not be valid after stop
    seq->active.on_status_context = NULL;
    free_clip(&seq->active);
}

static void resume_clip(ysw_seq_t *seq)
{
    ESP_LOGD(TAG, "resume_clip next_note=%d, note_count=%d", seq->next_note, seq->active.note_count);
    if (seq->active.note_count) {
        adjust_playback_start_millis(seq);
    }
}

static void loop_next(ysw_seq_t *seq)
{
    ESP_LOGD(TAG, "loop_next staged.note_count=%d", seq->staged.note_count);
    if (seq->staged.notes) {
        play_clip(seq, &seq->staged);
    } else {
        resume_clip(seq);
    }
}

static void set_tempo(ysw_seq_t *seq, uint8_t new_tempo)
{
    ESP_LOGW(TAG, "set_tempo tempo=%d", new_tempo);
    seq->active.tempo = new_tempo;
}

static void set_loop(ysw_seq_t *seq, bool new_value)
{
    seq->loop = new_value;
}

static void set_playback_speed(ysw_seq_t *seq, uint8_t percent)
{
    seq->playback_speed = percent;
    if (clip_playing(seq)) {
        adjust_playback_start_millis(seq);
    }
}

static void process_message(ysw_seq_t *seq, ysw_seq_message_t *message)
{
    switch (message->type) {
        case YSW_SEQ_PLAY:
            play_clip(seq, &message->play);
            break;
        case YSW_SEQ_PAUSE:
            pause_clip(seq);
            break;
        case YSW_SEQ_RESUME:
            resume_clip(seq);
            break;
        case YSW_SEQ_STOP:
            stop_clip(seq);
            break;
        case YSW_SEQ_TEMPO:
            set_tempo(seq, message->tempo.qnpm);
            break;
        case YSW_SEQ_LOOP:
            set_loop(seq, message->loop.loop);
            break;
        case YSW_SEQ_STAGE:
            stage_clip(seq, &message->stage);
            break;
        case YSW_SEQ_SPEED:
            set_playback_speed(seq, message->speed.percent);
            break;
        default:
            ESP_LOGW(TAG, "invalid message type=%d", message->type);
            break;
    }
}

static TickType_t process_notes(ysw_seq_t *seq)
{
    TickType_t ticks_to_wait = portMAX_DELAY;
    uint32_t playback_millis = get_current_playback_millis(seq);

    ysw_note_t *note;
    if (seq->next_note < seq->active.note_count) {
        note = &seq->active.notes[seq->next_note];
    } else {
        note = NULL;
    }

    int next_note_index = -1;
    active_note_t *next_note_to_end = NULL;

    uint8_t index = 0;
    while (index < seq->active_count) {
        active_note_t *active_note = &seq->active_notes[index];
        if (active_note->end_time < playback_millis) {
            seq->config.on_note_off(active_note->channel, active_note->midi_note);
            if (index + 1 < seq->active_count) {
                // replaced expired note with last one in array
                seq->active_notes[index] = seq->active_notes[seq->active_count - 1];
            }
            // free last member of array
            seq->active_count--;
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
        uint32_t note_start_time = t2ms(seq, note->start);
        if (note_start_time <= playback_millis) {
            play_note(seq, note, next_note_index);
            seq->next_note++;
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
        } else if (seq->loop) {
            fire_loop_done(seq);
            seq->next_note = 0;
            loop_next(seq);
            ticks_to_wait = 0;
        } else if (seq->staged.notes) {
            ESP_LOGD(TAG, "playback complete, playing staged clip");
            play_clip(seq, &seq->staged);
            ticks_to_wait = 0;
        } else {
            seq->next_note = 0;
            seq->start_millis = 0;
            ESP_LOGD(TAG, "playback complete, nothing more to do");
            fire_play_done(seq);
        }
    }

    return ticks_to_wait;
}

static void run_task(ysw_seq_t *seq)
{
    ESP_LOGD(TAG, "run_task core=%d", xPortGetCoreID());
    BaseType_t is_message = false;
    ysw_seq_message_t message = (ysw_seq_message_t){};
    for (;;) {
        TickType_t ticks_to_wait = portMAX_DELAY;
        if (clip_playing(seq)) {
            ticks_to_wait = process_notes(seq);
        }
        if (ticks_to_wait == portMAX_DELAY) {
            ESP_LOGD(TAG, "sequencer idle");
            fire_idle(seq);
        }
        if (is_message && message.rendezvous) {
            ESP_LOGD(TAG, "notifying sender");
            xEventGroupSetBits(message.rendezvous, 1);
        }
        is_message = xQueueReceive(seq->input_queue, &message, ticks_to_wait);
        if (is_message) {
            process_message(seq, &message);
        }
    }
}

QueueHandle_t ysw_seq_create_task(ysw_seq_config_t *seq_config)
{
    ysw_seq_t *seq = ysw_heap_allocate(sizeof(ysw_seq_t));
    *seq = (ysw_seq_t) {
        .config = *seq_config,
        .playback_speed = YSW_SEQ_SPEED_DEFAULT,
    };
    ysw_task_config_t task_config = {
        .name = TAG,
        .function = (TaskFunction_t)run_task,
        .parameters = seq,
        .queue = &seq->input_queue,
        .item_size = sizeof(ysw_seq_message_t),
        .queue_size = YSW_TASK_MEDIUM_QUEUE,
        .stack_size = YSW_TASK_MEDIUM_STACK,
        .priority = YSW_TASK_DEFAULT_PRIORITY,
    };
    ysw_task_create(&task_config);
    return seq->input_queue;
}
