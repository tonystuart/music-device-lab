// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_touch.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "ysw_i2c.h"
#include "ysw_mpr121.h"
#include "ysw_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG "YSW_TOUCH"

typedef struct {
    ysw_bus_t *bus;
    i2c_port_t port;
    gpio_num_t sda;
    gpio_num_t scl;
    ysw_array_t *addresses;
} ysw_touch_t;

static void fire_key_events(ysw_touch_t *touch, uint8_t scan_code)
{
    uint32_t current_millis = ysw_get_millis();
    ysw_event_key_down_t key_down = {
        .scan_code = scan_code,
        .time = current_millis - 250,
    };
    ysw_event_fire_key_down(touch->bus, &key_down);
    ysw_event_key_pressed_t key_pressed = {
        .scan_code = scan_code,
        .time = current_millis,
        .duration = 250,
        .repeat_count = 0,
    };
    ysw_event_fire_key_pressed(touch->bus, &key_pressed);
    ysw_event_key_up_t key_up = {
        .scan_code = scan_code,
        .time = current_millis,
        .duration = 250,
        .repeat_count = 0,
    };
    ysw_event_fire_key_up(touch->bus, &key_up);
}

static void initialize(void *context)
{
    ESP_LOGD(TAG, "initialize entered");
    ysw_touch_t *touch = context;
    ysw_i2c_init(touch->port, touch->sda, touch->scl);
    uint8_t address_count = ysw_array_get_count(touch->addresses);
    for (uint8_t i = 0; i < address_count; i++) {
        uint8_t address = (uintptr_t)ysw_array_get(touch->addresses, i);
        ysw_mpr121_initialize(touch->port, address);
    }
}

static void scan_touch_sensors(ysw_touch_t *touch)
{
    uint8_t address_count = ysw_array_get_count(touch->addresses);
    for (uint8_t i = 0; i < address_count; i++) {
        uint8_t address = (uintptr_t)ysw_array_get(touch->addresses, i);
        uint16_t touches = ysw_mpr121_get_touches(touch->port, address);
        if (touches) {
            ESP_LOGD(TAG, "address=%#x, touches=%#x", address, touches);
        }
    }
}

static void process_event(void *context, ysw_event_t *event)
{
    ysw_touch_t *touch = context;
    if (event) {
        switch (event->header.type) {
            default:
                break;
        }
    }
    scan_touch_sensors(touch);
}

void ysw_touch_create_task(ysw_bus_t *bus, i2c_port_t port, gpio_num_t sda, gpio_num_t scl, ysw_array_t *addresses)
{
    ysw_touch_t *touch = ysw_heap_allocate(sizeof(ysw_touch_t));
    touch->bus = bus;
    touch->port = port;
    touch->sda = sda;
    touch->sda = scl;
    touch->addresses = addresses;

    ysw_task_config_t config = ysw_task_default_config;
    config.name = TAG;
    config.bus = bus;
    config.initializer = initialize;
    config.event_handler = process_event;
    config.context = touch;
    config.wait_millis = 10;

    ysw_task_create(&config);
}
