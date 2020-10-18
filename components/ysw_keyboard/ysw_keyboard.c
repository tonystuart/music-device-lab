// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.


#include "ysw_keyboard.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG "KEYBOARD"

#define LOW 0
#define HIGH 1

typedef struct {
    uint32_t down_time;
    uint32_t repeat_count;
} state_t;

typedef struct {
    ysw_bus_h bus;
    ysw_array_t *rows;
    ysw_array_t *columns;
    state_t state[];
} context_t;

static void fire_key_down(context_t *context, uint8_t key)
{
    ESP_LOGD(TAG, "fire_key_down key=%d", key);
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_KEYBOARD,
        .header.type = YSW_EVENT_KEY_DOWN,
        .key_down.key = key,
    };
    ysw_event_publish(context->bus, &event);
}

static void fire_key_pressed(context_t *context, uint8_t key, uint32_t repeat_count)
{
    ESP_LOGD(TAG, "fire_key_pressed key=%d, repeat_count=%d", key, repeat_count);
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_KEYBOARD,
        .header.type = YSW_EVENT_KEY_PRESSED,
        .key_pressed.key = key,
        .key_pressed.repeat_count = repeat_count
    };
    ysw_event_publish(context->bus, &event);
}

static void fire_key_up(context_t *context, uint8_t key, uint32_t repeat_count)
{
    ESP_LOGD(TAG, "fire_key_up key=%d, repeat_count=%d", key, repeat_count);
    ysw_event_t event = {
        .header.origin = YSW_ORIGIN_KEYBOARD,
        .header.type = YSW_EVENT_KEY_UP,
        .key_up.key = key,
        .key_up.repeat_count = repeat_count,
    };
    ysw_event_publish(context->bus, &event);
}

static void configure_row(uint8_t gpio)
{
    ESP_LOGD(TAG, "configure_row gpio=%d", gpio);
    gpio_config_t config = {
        .pin_bit_mask = 1ull << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };

    $(gpio_config(&config));
}

static void configure_column(uint8_t gpio)
{
    ESP_LOGD(TAG, "configure_column gpio=%d", gpio);
    gpio_config_t config = {
        .pin_bit_mask = 1ull << gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };

    $(gpio_config(&config));
    $(gpio_set_level(gpio, HIGH));
}

void scan_keyboard(context_t *context)
{
    uint32_t row_count = ysw_array_get_count(context->rows);
    uint32_t column_count = ysw_array_get_count(context->columns);
    //ESP_LOGD(TAG, "scan_keyboard row_count=%d, column_count=%d", row_count, column_count);
    for (uint32_t column_index = 0; column_index < column_count; column_index++) {
        uint8_t column_gpio = (uintptr_t)ysw_array_get(context->columns, column_index);
        $(gpio_set_level(column_gpio, LOW));
        for (int row_index = 0; row_index < row_count; row_index++) {
            uint32_t current_millis = ysw_get_millis();
            uint8_t key = (row_index * column_count) + column_index;
            uint8_t row_gpio = (uintptr_t)ysw_array_get(context->rows, row_index);
            int key_pressed = gpio_get_level(row_gpio) == LOW;
            if (key_pressed) {
                //ESP_LOGD(TAG, "col=%d, row=%d, key=%d", column_index, row_index, key);
                if (!context->state[key].down_time) {
                    context->state[key].down_time = current_millis;
                    context->state[key].repeat_count = 0;
                    fire_key_down(context, key);
                } else if (context->state[key].down_time + 250 < current_millis) {
                    context->state[key].down_time = current_millis;
                    context->state[key].repeat_count++;
                    fire_key_pressed(context, key, context->state[key].repeat_count);
                }
            } else if (context->state[key].down_time) {
                context->state[key].down_time = 0;
                if (!context->state[key].repeat_count) {
                    fire_key_pressed(context, key, context->state[key].repeat_count);
                }
                fire_key_up(context, key, context->state[key].repeat_count);
            }
        }
        $(gpio_set_level(column_gpio, HIGH));
    }
}

static void on_play(context_t *context, ysw_event_play_t *event)
{
}

static void process_event(void *caller_context, ysw_event_t *event)
{
    context_t *context = caller_context;
    if (event) {
        switch (event->header.type) {
            case YSW_EVENT_PLAY:
                on_play(context, &event->play);
                break;
            default:
                break;
        }
    }
    scan_keyboard(context);
}

void ysw_keyboard_create_task(ysw_bus_h bus, ysw_keyboard_config_t *keyboard_config)
{
    uint32_t row_count = ysw_array_get_count(keyboard_config->rows);
    ESP_LOGD(TAG, "ysw_keyboard_create_task row_count=%d", row_count);
    for (int row_index = 0; row_index < row_count; row_index++) {
        uint8_t row_gpio = (uintptr_t)ysw_array_get(keyboard_config->rows, row_index);
        configure_row(row_gpio);
    }

    uint32_t column_count = ysw_array_get_count(keyboard_config->columns);
    ESP_LOGD(TAG, "ysw_keyboard_create_task column_count=%d", column_count);
    for (int column_index = 0; column_index < column_count; column_index++) {
        uint8_t column_gpio = (uintptr_t)ysw_array_get(keyboard_config->columns, column_index);
        configure_column(column_gpio);
    }

    context_t *context = ysw_heap_allocate(sizeof(context_t) +
            (sizeof(state_t) * row_count * column_count));

    context->bus = bus;
    context->rows = keyboard_config->rows;
    context->columns = keyboard_config->columns;

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.event_handler = process_event;
    config.caller_context = context;
    config.wait_millis = 10;

    ysw_task_h task = ysw_task_create(&config);
    ysw_task_subscribe(task, YSW_ORIGIN_COMMAND);
}
