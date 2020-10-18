// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_sequencer.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_task.h"
#include "ysw_midi.h"
#include "ysw_ticks.h"
#include "esp_log.h"
#include "time.h"

#define TAG "YSW_SEQUENCER"

#define MAX_POLYPHONY 64

typedef struct {
    uint8_t channel;
    uint8_t midi_note;
    time_t end_time;
} active_note_t;

typedef struct {
    ysw_bus_h bus;
    ysw_task_h task;
    ysw_event_clip_t clip;
    ysw_array_t *play_list;
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
    return ysw_ticks_to_millis_by_tpqn(ticks, context->clip.tempo, YSW_TICKS_DEFAULT_TPQN);
}

static inline bool is_clip_playing(context_t *context)
{
    return context->start_millis;
}

static void release_notes(context_t *context)
{
    for (uint8_t i = 0; i < context->active_count; i++) {
        active_note_t *active_note = &context->active_notes[i];
        fire_note_off(context, active_note->channel, active_note->midi_note);
    }
    context->active_count = 0;
}

static inline bool is_clip_present(context_t *context)
{
    return context->clip.notes;
}

static void free_clip(context_t *context)
{
    ysw_array_free(context->clip.notes);
    context->clip.notes = NULL;
    context->clip.tempo = 0;
}

static inline void adjust_playback_start_millis(context_t *context)
{
    uint32_t tick;
    if (context->next_note == 0) {
        // beginning of clip: start at zero
        tick = 0;
    } else {
        // middle of clip: start at current note
        ysw_note_t *note = ysw_array_get(context->clip.notes, context->next_note);
        tick = note->start;
    }

    uint32_t old_elapsed_millis = t2ms(context, tick);
    uint32_t new_elapsed_millis = (100 * old_elapsed_millis) / context->playback_speed;
    context->start_millis = ysw_get_millis() - new_elapsed_millis;
}

static uint32_t get_current_playback_millis(context_t *context)
{
    uint32_t current_millis = ysw_get_millis();
    uint32_t elapsed_millis = current_millis - context->start_millis;
    uint32_t playback_millis = (elapsed_millis * context->playback_speed) / 100;
    return playback_millis;
}

static void play_clip(context_t *context, ysw_event_clip_t *new_clip)
{
    ESP_LOGD(TAG, "play_clip tempo=%d", new_clip->tempo);
    if (is_clip_playing(context)) {
        release_notes(context);
    }
    if (is_clip_present(context)) {
        free_clip(context);
    }
    context->clip = *new_clip;
    context->next_note = 0;
    adjust_playback_start_millis(context);
}

static void pause_clip(context_t *context)
{
    ESP_LOGD(TAG, "pause_clip start_millis=%d", context->start_millis);
    if (is_clip_playing(context)) {
        release_notes(context);
    } else {
        // Hitting PAUSE twice is like STOP -- you restart at beginning
        context->next_note = 0;
    }
    context->start_millis = 0;
}

static void stop_clip(context_t *context)
{
    ESP_LOGD(TAG, "stop_clip start_millis=%d", context->start_millis);
    if (is_clip_playing(context)) {
        release_notes(context);
    }
    if (is_clip_present(context)) {
        free_clip(context);
    }
    context->next_note = 0;
    context->start_millis = 0;
}

static void resume_clip(context_t *context)
{
    ESP_LOGD(TAG, "resume_clip next_note=%d", context->next_note);
    if (is_clip_present(context)) {
        adjust_playback_start_millis(context);
    }
}

static void set_tempo(context_t *context, uint8_t new_tempo)
{
    ESP_LOGW(TAG, "set_tempo tempo=%d", new_tempo);
    context->clip.tempo = new_tempo;
}

static void set_loop(context_t *context, bool new_value)
{
    context->loop = new_value;
}

static void set_playback_speed(context_t *context, uint8_t percent)
{
    context->playback_speed = percent;
    if (is_clip_playing(context)) {
        adjust_playback_start_millis(context);
    }
}

static bool is_play_list_available(context_t *context)
{
    return ysw_array_get_count(context->play_list);
}

static void add_clip_to_play_list(context_t *context, ysw_event_clip_t *clip)
{
    ysw_event_clip_t *play_list_clip = ysw_heap_allocate(sizeof(ysw_event_clip_t));
    *play_list_clip = *clip;
    ysw_array_push(context->play_list, play_list_clip);
}

static void play_clip_from_play_list(context_t *context)
{
    ysw_event_clip_t *clip = ysw_array_remove(context->play_list, 0);
    play_clip(context, clip);
    ysw_heap_free(clip); // just the clip wrapper
}

static void clear_play_list(context_t *context)
{
    while (ysw_array_get_count(context->play_list)) {
        ysw_event_clip_t *clip = ysw_array_pop(context->play_list);
        ysw_array_free(clip->notes);
        ysw_heap_free(clip);
    }
}

static void on_play(context_t *context, ysw_event_play_t *event)
{
    if (event->type == YSW_EVENT_PLAY_NOW || !is_clip_playing(context)) {
        play_clip(context, &event->clip);
    } else {
        if (event->type == YSW_EVENT_PLAY_STAGE) {
            clear_play_list(context);
        }
        add_clip_to_play_list(context, &event->clip);
    }
}

static void play_note(context_t *context, ysw_note_t *note, int next_note_index)
{
    if (note->instrument != context->programs[note->channel]) {
        fire_program_change(context, note->channel, note->instrument);
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

static TickType_t process_notes(context_t *context)
{
    TickType_t ticks_to_wait = portMAX_DELAY;
    uint32_t playback_millis = get_current_playback_millis(context);

    ysw_note_t *note;
    if (context->next_note < ysw_array_get_count(context->clip.notes)) {
        note = ysw_array_get(context->clip.notes, context->next_note);
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
            ticks_to_wait = ysw_millis_to_ticks(delay_millis);
        }
    } else {
        if (next_note_to_end) {
            ESP_LOGD(TAG, "song complete, waiting for notes to end");
            uint32_t delay_millis = next_note_to_end->end_time - playback_millis;
            ticks_to_wait = ysw_millis_to_ticks(delay_millis);
        } else if (context->loop) {
            ESP_LOGD(TAG, "loop complete, looping to start");
            fire_loop_done(context);
            context->next_note = 0;
            resume_clip(context);
            ticks_to_wait = 0;
        } else if (is_play_list_available(context)) {
            ESP_LOGD(TAG, "playback complete, playing next from play_list");
            play_clip_from_play_list(context);
            ticks_to_wait = 0;
        } else {
            ESP_LOGD(TAG, "playback complete, nothing more to do");
            context->next_note = 0;
            context->start_millis = 0;
            fire_play_done(context);
        }
    }

    return ticks_to_wait;
}

static void process_event(void *caller_context, ysw_event_t *event)
{
    context_t *context = caller_context;
    if (event) {
        switch (event->header.type) {
            case YSW_EVENT_PLAY:
                on_play(context, &event->play);
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
            case YSW_EVENT_SPEED:
                set_playback_speed(context, event->speed.percent);
                break;
            default:
                break;
        }
    }
    TickType_t ticks_to_wait = portMAX_DELAY;
    if (is_clip_playing(context)) {
        ticks_to_wait = process_notes(context);
        if (ticks_to_wait == portMAX_DELAY) {
            ESP_LOGD(TAG, "sequencer idle");
            fire_idle(context);
        }
    }
    ysw_task_set_wait_millis(context->task, ticks_to_wait);
}

void ysw_sequencer_create_task(ysw_bus_h bus)
{
    context_t *context = ysw_heap_allocate(sizeof(context_t));

    context->bus = bus;
    context->play_list = ysw_array_create(4);
    context->playback_speed = YSW_SEQUENCER_SPEED_DEFAULT;

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.task = &context->task;
    config.event_handler = process_event;
    config.caller_context = context;

    ysw_task_create(&config);
    ysw_task_subscribe(context->task, YSW_ORIGIN_COMMAND);
}

