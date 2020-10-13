// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_seq.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_task.h"
#include "ysw_midi.h"
#include "ysw_ticks.h"
#include "esp_log.h"
#include "time.h"

#define TAG "YSW_SEQ"

#define MAX_POLYPHONY 64

typedef struct {
    uint8_t channel;
    uint8_t midi_note;
    time_t end_time;
} active_note_t;

typedef struct {
    ysw_bus_h bus;
    QueueHandle_t input_queue;
    ysw_event_clip_t playing;
    ysw_event_clip_t staging;
    active_note_t active_notes[MAX_POLYPHONY];
    uint8_t programs[YSW_MIDI_MAX_CHANNELS];
    uint32_t next_note;
    uint32_t start_millis;
    uint8_t active_count;
    uint8_t playback_speed;
    bool loop;
} context_t;

static void fire_note_on(context_t *context, uint8_t channel, uint8_t midi_note, uint8_t velocity)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_SEQUENCER,
        .header.type = YSW_EVENT_NOTE_ON,
        .note_on.channel = channel,
        .note_on.midi_note = midi_note,
        .note_on.velocity = velocity,
    };
    ysw_event_publish(context->bus, &event);
}

static void fire_note_off(context_t *context, uint8_t channel, uint8_t midi_note)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_SEQUENCER,
        .header.type = YSW_EVENT_NOTE_OFF,
        .note_off.channel = channel,
        .note_off.midi_note = midi_note,
    };
    ysw_event_publish(context->bus, &event);
}

static void fire_program_change(context_t *context, uint8_t channel, uint8_t program)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_SEQUENCER,
        .header.type = YSW_EVENT_PROGRAM_CHANGE,
        .program_change.channel = channel,
        .program_change.program = program,
    };
    ysw_event_publish(context->bus, &event);
}

static void fire_loop_done(context_t *context)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_SEQUENCER,
        .header.type = YSW_EVENT_LOOP_DONE,
    };
    ysw_event_publish(context->bus, &event);
}

static void fire_play_done(context_t *context)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_SEQUENCER,
        .header.type = YSW_EVENT_PLAY_DONE,
    };
    ysw_event_publish(context->bus, &event);
}

static void fire_idle(context_t *context)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_SEQUENCER,
        .header.type = YSW_EVENT_IDLE,
    };
    ysw_event_publish(context->bus, &event);
}

static void fire_note_status(context_t *context, ysw_note_t *note)
{
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_SEQUENCER,
        .header.type = YSW_EVENT_NOTE_STATUS,
        .note_status.note = *note,
    };
    ysw_event_publish(context->bus, &event);
}

static inline time_t t2ms(context_t *context, uint32_t ticks)
{
    return ysw_ticks_to_millis_by_tpqn(ticks, context->playing.tempo, YSW_TICKS_DEFAULT_TPQN);
}

static inline bool clip_playing(context_t *context)
{
    return context->start_millis;
}

static void free_clip(ysw_event_clip_t *clip)
{
    if (clip->notes) {
        ysw_array_free(clip->notes);
        clip->notes = NULL;
        clip->tempo = 0;
    }
}

static void release_all_notes(context_t *context)
{
    for (uint8_t i = 0; i < context->active_count; i++) {
        active_note_t *active_note = &context->active_notes[i];
        fire_note_off(context, active_note->channel, active_note->midi_note);
    }
    context->active_count = 0;
}

static void play_note(context_t *context, ysw_note_t *note, int next_note_index)
{
    if (note->instrument != context->programs[note->channel]) {
#if YSW_MAIN_SYNTH_MODEL == 4
        // No special treatment for channel 9 with ysw_synth_mod
        fire_program_change(context, note->channel, note->instrument);
#else
        if (note->channel != YSW_MIDI_DRUM_CHANNEL) {
            fire_program_change(context, note->channel, note->instrument);
        }
#endif
        context->programs[note->channel] = note->instrument;
    }

    if (next_note_index != -1) {
        fire_note_off(context, note->channel, note->midi_note);
    } else {
        if (context->active_count < MAX_POLYPHONY) {
            next_note_index = context->active_count++;
        }
    }

    if (next_note_index != -1) {
        if (note->channel == YSW_MIDI_STATUS_CHANNEL) {
            fire_note_status(context, note);
        } else {
            fire_note_on(context, note->channel, note->midi_note, note->velocity);
            context->active_notes[next_note_index].channel = note->channel;
            context->active_notes[next_note_index].midi_note = note->midi_note;
            context->active_notes[next_note_index].end_time = t2ms(context, note->start) + t2ms(context, note->duration);
            //ESP_LOGD(TAG, "active_notes[%d].end_time=%ld", next_note_index, context->active_notes[next_note_index].end_time);
        }
    } else {
        ESP_LOGE(TAG, "Maximum polyphony exceeded, active_count=%d", context->active_count);
    }
}

static inline void adjust_playback_start_millis(context_t *context)
{
    uint32_t tick;
    if (context->next_note == 0) {
        // beginning of clip: start at zero
        tick = 0;
    } else {
        // middle of clip: start at current note
        ysw_note_t *note = ysw_array_get_fast(context->playing.notes, context->next_note);
        tick = note->start;
    }

    uint32_t old_elapsed_millis = t2ms(context, tick);
    uint32_t new_elapsed_millis = (100 * old_elapsed_millis) / context->playback_speed;
    context->start_millis = get_millis() - new_elapsed_millis;
}

static uint32_t get_current_playback_millis(context_t *context)
{
    uint32_t current_millis = get_millis();
    uint32_t elapsed_millis = current_millis - context->start_millis;
    uint32_t playback_millis = (elapsed_millis * context->playback_speed) / 100;
    return playback_millis;
}

static void play_clip(context_t *context, ysw_event_clip_t *new_clip)
{
    ESP_LOGD(TAG, "play_clip tempo=%d", new_clip->tempo);
    if (clip_playing(context)) {
        ESP_LOGD(TAG, "play_clip releasing notes");
        release_all_notes(context);
    }
    free_clip(&context->playing);
    context->playing = *new_clip;
    if (context->playing.notes == context->staging.notes) {
        // playing staging clip: don't free them, just unstage them
        ESP_LOGD(TAG, "play_clip playing staging clip");
        context->staging.notes = NULL;
        context->staging.tempo = 0;
    } else {
        // playing new clip: clear any staging clip
        ESP_LOGD(TAG, "play_clip playing new clip");
        free_clip(&context->staging);
    }
    context->next_note = 0;
    if (context->playing.notes) {
        ESP_LOGD(TAG, "play_clip adjusting start millis");
        adjust_playback_start_millis(context);
    }
    ESP_LOGD(TAG, "play_clip start_millis=%d", context->start_millis);
}

static void stage_clip(context_t *context, ysw_event_clip_t *new_clip)
{
    ESP_LOGD(TAG, "stage_clip tempo=%d", new_clip->tempo);
    if (clip_playing(context)) {
        free_clip(&context->staging);
        context->staging = *new_clip;
    } else {
        play_clip(context, new_clip);
    }
}

static void pause_clip(context_t *context)
{
    ESP_LOGD(TAG, "pause_clip start_millis=%d", context->start_millis);
    if (clip_playing(context)) {
        release_all_notes(context);
    } else {
        // Hitting PAUSE twice is like STOP -- you restart at beginning
        context->next_note = 0;
    }
    context->start_millis = 0;
}

static void stop_clip(context_t *context)
{
    ESP_LOGD(TAG, "stop_clip start_millis=%d", context->start_millis);
    if (clip_playing(context)) {
        release_all_notes(context);
    }
    context->next_note = 0;
    context->start_millis = 0;
    free_clip(&context->playing);
}

static void resume_clip(context_t *context)
{
    ESP_LOGD(TAG, "resume_clip next_note=%d", context->next_note);
    if (context->playing.notes) {
        adjust_playback_start_millis(context);
    }
}

static void loop_next(context_t *context)
{
    ESP_LOGD(TAG, "loop_next");
    if (context->staging.notes) {
        play_clip(context, &context->staging);
    } else {
        resume_clip(context);
    }
}

static void set_tempo(context_t *context, uint8_t new_tempo)
{
    ESP_LOGW(TAG, "set_tempo tempo=%d", new_tempo);
    context->playing.tempo = new_tempo;
}

static void set_loop(context_t *context, bool new_value)
{
    context->loop = new_value;
}

static void set_playback_speed(context_t *context, uint8_t percent)
{
    context->playback_speed = percent;
    if (clip_playing(context)) {
        adjust_playback_start_millis(context);
    }
}

static void process_event(context_t *context, ysw_event_t *event)
{
    switch (event->header.type) {
        case YSW_EVENT_PLAY:
            play_clip(context, &event->play);
            break;
        case YSW_EVENT_PAUSE:
            pause_clip(context);
            break;
        case YSW_EVENT_RESUME:
            resume_clip(context);
            break;
        case YSW_EVENT_STOP:
            stop_clip(context);
            break;
        case YSW_EVENT_TEMPO:
            set_tempo(context, event->tempo.qnpm);
            break;
        case YSW_EVENT_LOOP:
            set_loop(context, event->loop.loop);
            break;
        case YSW_EVENT_STAGE:
            stage_clip(context, &event->stage);
            break;
        case YSW_EVENT_SPEED:
            set_playback_speed(context, event->speed.percent);
            break;
        default:
            break;
    }
}

static TickType_t process_notes(context_t *context)
{
    TickType_t ticks_to_wait = portMAX_DELAY;
    uint32_t playback_millis = get_current_playback_millis(context);

    ysw_note_t *note;
    if (context->next_note < ysw_array_get_count_fast(context->playing.notes)) {
        note = ysw_array_get_fast(context->playing.notes, context->next_note);
    } else {
        note = NULL;
    }

    int next_note_index = -1;
    active_note_t *next_note_to_end = NULL;

    uint8_t index = 0;
    while (index < context->active_count) {
        active_note_t *active_note = &context->active_notes[index];
        if (active_note->end_time <= playback_millis) {
            fire_note_off(context, active_note->channel, active_note->midi_note);
            if (index + 1 < context->active_count) {
                // replaced expired note with last one in array
                context->active_notes[index] = context->active_notes[context->active_count - 1];
            }
            // free last member of array
            context->active_count--;
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
        uint32_t note_start_time = t2ms(context, note->start);
        if (note_start_time <= playback_millis) {
            play_note(context, note, next_note_index);
            context->next_note++;
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
            uint32_t delay_millis = next_note_to_end->end_time - playback_millis;
            ESP_LOGD(TAG, "waiting for final notes to end, end_time=%ld, current_time=%d", next_note_to_end->end_time, playback_millis);
            ticks_to_wait = to_ticks(delay_millis);
        } else if (context->loop) {
            fire_loop_done(context);
            context->next_note = 0;
            loop_next(context);
            ticks_to_wait = 0;
        } else if (context->staging.notes) {
            ESP_LOGD(TAG, "playback complete, playing staging clip");
            play_clip(context, &context->staging);
            ticks_to_wait = 0;
        } else {
            context->next_note = 0;
            context->start_millis = 0;
            ESP_LOGD(TAG, "playback complete, nothing more to do");
            fire_play_done(context);
        }
    }

    return ticks_to_wait;
}

static void task_handler(context_t *context)
{
    BaseType_t is_event = false;
    ysw_event_t event = (ysw_event_t){};
    for (;;) {
        TickType_t ticks_to_wait = portMAX_DELAY;
        if (clip_playing(context)) {
            ticks_to_wait = process_notes(context);
        }
        if (ticks_to_wait == portMAX_DELAY) {
            ESP_LOGD(TAG, "sequencer idle");
            fire_idle(context);
        }
        is_event = xQueueReceive(context->input_queue, &event, ticks_to_wait);
        if (is_event) {
            process_event(context, &event);
        }
    }
}

void ysw_seq_create_task(ysw_bus_h bus)
{
    context_t *context = ysw_heap_allocate(sizeof(context_t));

    context->bus = bus;
    context->playback_speed = YSW_SEQ_SPEED_DEFAULT;

    ysw_task_config_t task_config = {
        .name = TAG,
        .function = (TaskFunction_t)task_handler,
        .parameters = context,
        .queue = &context->input_queue,
        .item_size = sizeof(ysw_event_t),
        .queue_size = YSW_TASK_MEDIUM_QUEUE,
        .stack_size = YSW_TASK_MEDIUM_STACK,
        .priority = YSW_TASK_DEFAULT_PRIORITY,
    };

    ysw_task_create(&task_config);

    ysw_bus_subscribe(bus, YSW_ORIGIN_COMMAND, context->input_queue);
}
