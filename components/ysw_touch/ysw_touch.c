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
#include "ysw_keystate.h"
#include "ysw_mpr121.h"
#include "ysw_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "stdint.h"

#define TAG "YSW_TOUCH"

#define SENSORS_PER_MPR121 12

typedef struct {
    ysw_bus_t *bus;
    ysw_i2c_t *i2c;
    ysw_array_t *addresses;
    ysw_keystate_t *keystate;
    uint16_t previous_touches[];
} ysw_touch_t;

static void initialize(void *context)
{
    ysw_touch_t *touch = context;
    uint8_t address_count = ysw_array_get_count(touch->addresses);
    for (uint8_t i = 0; i < address_count; i++) {
        uint8_t address = (uintptr_t)ysw_array_get(touch->addresses, i);
        ysw_mpr121_initialize(touch->i2c, address);
    }
}

static void process_touches(ysw_touch_t *touch, uint8_t mpr121_index, uint16_t touches)
{
    for (uint8_t i = 0; i < SENSORS_PER_MPR121; i++) {
        uint8_t button_index = (mpr121_index * SENSORS_PER_MPR121) + i;
        bool pressed = touches & (1 << i);
        if (pressed) {
            ESP_LOGD(TAG, "press button_index=%d", button_index);
            ysw_keystate_on_press(touch->keystate, button_index);
        } else {
            ysw_keystate_on_release(touch->keystate, button_index);
        }
    }
}

static void scan_touch_sensors(ysw_touch_t *touch)
{
    uint8_t address_count = ysw_array_get_count(touch->addresses);
    for (uint8_t i = 0; i < address_count; i++) {
        uint8_t address = (uintptr_t)ysw_array_get(touch->addresses, i);
        uint16_t touches = ysw_mpr121_get_touches(touch->i2c, address);
        if (touches /* handle repeat */ || touches != touch->previous_touches[i]) {
            //ESP_LOGD(TAG, "address=%#x, touches=%#x", address, touches);
            process_touches(touch, i, touches);
            touch->previous_touches[i] = touches;
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

void ysw_touch_create_task(ysw_bus_t *bus, ysw_i2c_t *i2c, ysw_array_t *addresses)
{
    uint8_t mpr121_count = ysw_array_get_count(addresses);
    uint32_t sensor_count = mpr121_count * SENSORS_PER_MPR121;
    uint32_t fam_size = mpr121_count * YSW_FIELD_SIZE(ysw_touch_t, previous_touches[0]);
    uint32_t touch_size = sizeof(ysw_touch_t) + fam_size;

    ysw_touch_t *touch = ysw_heap_allocate(touch_size);

    touch->bus = bus;
    touch->i2c = i2c;
    touch->addresses = addresses;
    touch->keystate = ysw_keystate_create(bus, sensor_count);

    ysw_task_config_t config = ysw_task_default_config;

    config.name = TAG;
    config.bus = bus;
    config.initializer = initialize;
    config.event_handler = process_event;
    config.context = touch;
    config.wait_millis = 10;

    ysw_task_create(&config);
}
