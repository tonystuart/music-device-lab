// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_led.h"
#include "ws2812.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_task.h"
#include "esp_log.h"

#define TAG "LED"

#define LED_COUNT 40
#define MAX_POWER 16

#define YSW_LED_RED ws2812_color(255, 0, 0)
#define YSW_LED_YELLOW ws2812_color(255, 255, 0)
#define YSW_LED_GREEN ws2812_color(0, 255, 0)
#define YSW_LED_CYAN ws2812_color(0, 255, 255)
#define YSW_LED_BLUE ws2812_color(0, 0, 255)
#define YSW_LED_MAGENTA ws2812_color(255, 0, 255)

static ws2812_color_t colors[3][2] = {
    { YSW_LED_CYAN, YSW_LED_BLUE, },
    { YSW_LED_YELLOW, YSW_LED_GREEN, },
    { YSW_LED_MAGENTA, YSW_LED_RED, },
};

//   00, 01,     02, 03, 04,     05, 06, 07, 08,
// 09, 10, 11, 12, 13, 14, 15,   16, 17, 18, 19,
//   20, 21,     22, 23, 24,     25, 26, 27, 28,
// 29, 30, 31, 32, 33, 34, 35,   36, 37, 38, 39,

static uint8_t semitone_leds[] = {
    29, 20, 30, 21, 31, 32, 22, 33, 23, 34, 24, 35,
    9,   0, 10,  1, 11, 12,  2, 13,  3, 14,  4, 15,
};

typedef struct {
} context_t;

static void on_play(context_t *context, ysw_event_play_t *event)
{
}

static void control_led(uint8_t channel, uint8_t midi_note, bool on)
{
    if (midi_note < 36 || midi_note > 83 || channel > 2) {
        return;
    }

    uint8_t bank = (midi_note - 36) / 24;
    uint8_t tone = (midi_note - 36) % 24;

    ws2812_color_t color;
    if (on) {
        color = colors[channel][bank];
    } else {
        color = ws2812_make_color(0, 0, 0);
    }
    uint8_t led = semitone_leds[tone];
    ws2812_set_color(led, color);
    ws2812_update_display();
}

static void on_note_on(context_t *context, ysw_event_note_on_t *event)
{
    control_led(event->channel, event->midi_note, true);
}

static void on_note_off(context_t *context, ysw_event_note_off_t *event)
{
    control_led(event->channel, event->midi_note, false);
}

static void process_event(void *caller_context, ysw_event_t *event)
{
    context_t *context = caller_context;
    if (event) {
        switch (event->header.type) {
            case YSW_EVENT_PLAY:
                on_play(context, &event->play);
                break;
            case YSW_EVENT_NOTE_ON:
                on_note_on(context, &event->note_on);
                break;
            case YSW_EVENT_NOTE_OFF:
                on_note_off(context, &event->note_off);
                break;
            default:
                break;
        }
    }
}

void ysw_led_create_task(ysw_bus_h bus, ysw_led_config_t *led_config)
{
    context_t *context = ysw_heap_allocate(sizeof(context_t));
    ws2812_initialize(led_config->gpio, LED_COUNT, MAX_POWER);

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.event_handler = process_event;
    config.caller_context = context;

    ysw_task_h task = ysw_task_create(&config);
    ysw_task_subscribe(task, YSW_ORIGIN_COMMAND);
    ysw_task_subscribe(task, YSW_ORIGIN_NOTE);
}
