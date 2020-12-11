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
    ysw_bus_t *bus;
    ysw_task_t *task;
    ysw_event_clip_t clip;
    ysw_array_t *play_list;
    active_note_t active_notes[MAX_POLYPHONY];
    uint8_t programs[YSW_MIDI_MAX_CHANNELS];
    uint32_t next_note;
    uint32_t start_millis;
    uint8_t active_count;
    uint8_t playback_speed;
    bool loop;
} ysw_sequencer_t;

static inline time_t t2ms(ysw_sequencer_t *ysw_sequencer, uint32_t ticks)
{
    return ysw_ticks_to_millis(ticks, ysw_sequencer->clip.bpm);
}

static inline bool is_clip_playing(ysw_sequencer_t *ysw_sequencer)
{
    return ysw_sequencer->start_millis;
}

static void release_notes(ysw_sequencer_t *ysw_sequencer)
{
    for (uint8_t i = 0; i < ysw_sequencer->active_count; i++) {
        active_note_t *active_note = &ysw_sequencer->active_notes[i];
        ysw_event_note_off_t note_off = {
            .channel = active_note->channel,
            .midi_note = active_note->midi_note,
        };
        ysw_event_fire_note_off(ysw_sequencer->bus, YSW_ORIGIN_SEQUENCER, &note_off);
    }
    ysw_sequencer->active_count = 0;
}

static inline bool is_clip_present(ysw_sequencer_t *ysw_sequencer)
{
    return ysw_sequencer->clip.notes;
}

static void free_clip(ysw_sequencer_t *ysw_sequencer)
{
    ysw_array_free(ysw_sequencer->clip.notes);
    ysw_sequencer->clip.notes = NULL;
    ysw_sequencer->clip.bpm = 0;
}

static inline void adjust_playback_start_millis(ysw_sequencer_t *ysw_sequencer)
{
    uint32_t tick;
    if (ysw_sequencer->next_note == 0) {
        // beginning of clip: start at zero
        tick = 0;
    } else {
        // middle of clip: start at current note
        ysw_note_t *note = ysw_array_get(ysw_sequencer->clip.notes, ysw_sequencer->next_note);
        tick = note->start;
    }

    uint32_t old_elapsed_millis = t2ms(ysw_sequencer, tick);
    uint32_t new_elapsed_millis = (100 * old_elapsed_millis) / ysw_sequencer->playback_speed;
    ysw_sequencer->start_millis = ysw_get_millis() - new_elapsed_millis;
}

static uint32_t get_current_playback_millis(ysw_sequencer_t *ysw_sequencer)
{
    uint32_t current_millis = ysw_get_millis();
    uint32_t elapsed_millis = current_millis - ysw_sequencer->start_millis;
    uint32_t playback_millis = (elapsed_millis * ysw_sequencer->playback_speed) / 100;
    return playback_millis;
}

static void play_clip(ysw_sequencer_t *ysw_sequencer, ysw_event_clip_t *new_clip)
{
    ESP_LOGD(TAG, "play_clip bpm=%d", new_clip->bpm);
    if (is_clip_playing(ysw_sequencer)) {
        release_notes(ysw_sequencer);
    }
    if (is_clip_present(ysw_sequencer)) {
        free_clip(ysw_sequencer);
    }
    ysw_sequencer->clip = *new_clip;
    ysw_sequencer->next_note = 0;
    adjust_playback_start_millis(ysw_sequencer);
}

static void pause_clip(ysw_sequencer_t *ysw_sequencer)
{
    ESP_LOGD(TAG, "pause_clip start_millis=%d", ysw_sequencer->start_millis);
    if (is_clip_playing(ysw_sequencer)) {
        release_notes(ysw_sequencer);
    } else {
        // Hitting PAUSE twice is like STOP -- you restart at beginning
        ysw_sequencer->next_note = 0;
    }
    ysw_sequencer->start_millis = 0;
}

static void stop_clip(ysw_sequencer_t *ysw_sequencer)
{
    ESP_LOGD(TAG, "stop_clip start_millis=%d", ysw_sequencer->start_millis);
    if (is_clip_playing(ysw_sequencer)) {
        release_notes(ysw_sequencer);
    }
    if (is_clip_present(ysw_sequencer)) {
        free_clip(ysw_sequencer);
    }
    ysw_sequencer->next_note = 0;
    ysw_sequencer->start_millis = 0;
}

static void resume_clip(ysw_sequencer_t *ysw_sequencer)
{
    ESP_LOGD(TAG, "resume_clip next_note=%d", ysw_sequencer->next_note);
    if (is_clip_present(ysw_sequencer)) {
        adjust_playback_start_millis(ysw_sequencer);
    }
}

static void set_tempo(ysw_sequencer_t *ysw_sequencer, uint8_t bpm)
{
    ESP_LOGW(TAG, "set_tempo bpm=%d", bpm);
    ysw_sequencer->clip.bpm = bpm;
}

static void set_loop(ysw_sequencer_t *ysw_sequencer, bool new_value)
{
    ysw_sequencer->loop = new_value;
}

static void set_playback_speed(ysw_sequencer_t *ysw_sequencer, uint8_t percent)
{
    ysw_sequencer->playback_speed = percent;
    if (is_clip_playing(ysw_sequencer)) {
        adjust_playback_start_millis(ysw_sequencer);
    }
}

static bool is_play_list_available(ysw_sequencer_t *ysw_sequencer)
{
    return ysw_array_get_count(ysw_sequencer->play_list);
}

static void add_clip_to_play_list(ysw_sequencer_t *ysw_sequencer, ysw_event_clip_t *clip)
{
    ysw_event_clip_t *play_list_clip = ysw_heap_allocate(sizeof(ysw_event_clip_t));
    *play_list_clip = *clip;
    ysw_array_push(ysw_sequencer->play_list, play_list_clip);
}

static void play_clip_from_play_list(ysw_sequencer_t *ysw_sequencer)
{
    ysw_event_clip_t *clip = ysw_array_remove(ysw_sequencer->play_list, 0);
    play_clip(ysw_sequencer, clip);
    ysw_heap_free(clip); // just the clip wrapper
}

static void clear_play_list(ysw_sequencer_t *ysw_sequencer)
{
    while (ysw_array_get_count(ysw_sequencer->play_list)) {
        ysw_event_clip_t *clip = ysw_array_pop(ysw_sequencer->play_list);
        ysw_array_free(clip->notes);
        ysw_heap_free(clip);
    }
}

static void on_play(ysw_sequencer_t *ysw_sequencer, ysw_event_play_t *event)
{
    if (event->type == YSW_EVENT_PLAY_NOW || !is_clip_playing(ysw_sequencer)) {
        play_clip(ysw_sequencer, &event->clip);
    } else {
        if (event->type == YSW_EVENT_PLAY_STAGE) {
            clear_play_list(ysw_sequencer);
        }
        add_clip_to_play_list(ysw_sequencer, &event->clip);
    }
}

static void play_note(ysw_sequencer_t *ysw_sequencer, ysw_note_t *note, int next_note_index)
{
    if (note->program != ysw_sequencer->programs[note->channel]) {
        ysw_event_program_change_t program_change = {
            .channel = note->channel,
            .program = note->program,
        };
        ysw_event_fire_program_change(ysw_sequencer->bus, YSW_ORIGIN_EDITOR, &program_change);
        ysw_sequencer->programs[note->channel] = note->program;
    }

    if (next_note_index != -1) {
        ysw_event_note_off_t note_off = {
            .channel = note->channel,
            .midi_note = note->midi_note,
        };
        ysw_event_fire_note_off(ysw_sequencer->bus, YSW_ORIGIN_SEQUENCER, &note_off);
    } else {
        if (ysw_sequencer->active_count < MAX_POLYPHONY) {
            next_note_index = ysw_sequencer->active_count++;
        }
    }

    if (next_note_index != -1) {
        ysw_event_note_on_t note_on = {
            .channel = note->channel,
            .midi_note = note->midi_note,
            .velocity = note->velocity,
        };
        ysw_event_fire_note_on(ysw_sequencer->bus, YSW_ORIGIN_SEQUENCER, &note_on);
        ysw_sequencer->active_notes[next_note_index].channel = note->channel;
        ysw_sequencer->active_notes[next_note_index].midi_note = note->midi_note;
        ysw_sequencer->active_notes[next_note_index].end_time =
            t2ms(ysw_sequencer, note->start) + t2ms(ysw_sequencer, note->duration);
        ysw_event_fire_note_status(ysw_sequencer->bus, note);
    } else {
        ESP_LOGE(TAG, "Maximum polyphony exceeded, active_count=%d", ysw_sequencer->active_count);
    }
}

static TickType_t process_notes(ysw_sequencer_t *ysw_sequencer)
{
    TickType_t ticks_to_wait = portMAX_DELAY;
    uint32_t playback_millis = get_current_playback_millis(ysw_sequencer);

    ysw_note_t *note;
    if (ysw_sequencer->next_note < ysw_array_get_count(ysw_sequencer->clip.notes)) {
        note = ysw_array_get(ysw_sequencer->clip.notes, ysw_sequencer->next_note);
    } else {
        note = NULL;
    }

    int next_note_index = -1;
    active_note_t *next_note_to_end = NULL;

    uint8_t index = 0;
    while (index < ysw_sequencer->active_count) {
        active_note_t *active_note = &ysw_sequencer->active_notes[index];
        if (active_note->end_time <= playback_millis) {
            ysw_event_note_off_t note_off = {
                .channel = active_note->channel,
                .midi_note = active_note->midi_note,
            };
            ysw_event_fire_note_off(ysw_sequencer->bus, YSW_ORIGIN_SEQUENCER, &note_off);
            if (index + 1 < ysw_sequencer->active_count) {
                // replaced expired note with last one in array
                ysw_sequencer->active_notes[index] = ysw_sequencer->active_notes[ysw_sequencer->active_count - 1];
            }
            // free last member of array
            ysw_sequencer->active_count--;
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
        uint32_t note_start_time = t2ms(ysw_sequencer, note->start);
        if (note_start_time <= playback_millis) {
            play_note(ysw_sequencer, note, next_note_index);
            ysw_sequencer->next_note++;
            ticks_to_wait = 0;
        } else {
            long time_of_next_event;
            if (next_note_to_end && (next_note_to_end->end_time < note_start_time)) {
                time_of_next_event = next_note_to_end->end_time;
            } else {
                time_of_next_event = note_start_time;
            }
            uint32_t delay_millis = time_of_next_event - playback_millis;
            ticks_to_wait = ysw_millis_to_rtos_ticks(delay_millis);
        }
    } else {
        if (next_note_to_end) {
            ESP_LOGD(TAG, "song complete, waiting for notes to end");
            uint32_t delay_millis = next_note_to_end->end_time - playback_millis;
            ticks_to_wait = ysw_millis_to_rtos_ticks(delay_millis);
        } else if (ysw_sequencer->loop) {
            ESP_LOGD(TAG, "loop complete, looping to start");
            ysw_event_fire_loop_done(ysw_sequencer->bus);
            ysw_sequencer->next_note = 0;
            resume_clip(ysw_sequencer);
            ticks_to_wait = 0;
        } else if (is_play_list_available(ysw_sequencer)) {
            ESP_LOGD(TAG, "playback complete, playing next from play_list");
            play_clip_from_play_list(ysw_sequencer);
            ticks_to_wait = 0;
        } else {
            ESP_LOGD(TAG, "playback complete, nothing more to do");
            ysw_sequencer->next_note = 0;
            ysw_sequencer->start_millis = 0;
            ysw_event_fire_play_done(ysw_sequencer->bus);
        }
    }

    return ticks_to_wait;
}

static void process_event(void *context, ysw_event_t *event)
{
    ysw_sequencer_t *ysw_sequencer = context;
    if (event) {
        switch (event->header.type) {
            case YSW_EVENT_PLAY:
                on_play(ysw_sequencer, &event->play);
                break;
            case YSW_EVENT_PAUSE:
                pause_clip(ysw_sequencer);
                break;
            case YSW_EVENT_RESUME:
                resume_clip(ysw_sequencer);
                break;
            case YSW_EVENT_STOP:
                stop_clip(ysw_sequencer);
                break;
            case YSW_EVENT_TEMPO:
                set_tempo(ysw_sequencer, event->tempo.bpm);
                break;
            case YSW_EVENT_LOOP:
                set_loop(ysw_sequencer, event->loop.loop);
                break;
            case YSW_EVENT_SPEED:
                set_playback_speed(ysw_sequencer, event->speed.percent);
                break;
            default:
                break;
        }
    }
    TickType_t ticks_to_wait = portMAX_DELAY;
    if (is_clip_playing(ysw_sequencer)) {
        ticks_to_wait = process_notes(ysw_sequencer);
        if (ticks_to_wait == portMAX_DELAY) {
            ESP_LOGD(TAG, "sequencer idle");
            ysw_event_fire_idle(ysw_sequencer->bus);
        }
    }
    ysw_task_set_wait_millis(ysw_sequencer->task, ticks_to_wait);
}

void ysw_sequencer_create_task(ysw_bus_t *bus)
{
    ysw_sequencer_t *ysw_sequencer = ysw_heap_allocate(sizeof(ysw_sequencer_t));

    ysw_sequencer->bus = bus;
    ysw_sequencer->play_list = ysw_array_create(4);
    ysw_sequencer->playback_speed = YSW_SEQUENCER_SPEED_DEFAULT;

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.task = &ysw_sequencer->task;
    config.event_handler = process_event;
    config.context = ysw_sequencer;

    ysw_task_create(&config);
    ysw_task_subscribe(ysw_sequencer->task, YSW_ORIGIN_COMMAND);
}

