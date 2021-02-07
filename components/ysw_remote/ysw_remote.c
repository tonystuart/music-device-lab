// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_remote.h"
#include "ysw_common.h"
#include "ysw_heap.h"
#include "ysw_task.h"
#include "driver/rmt.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "YSW_REMOTE"

#define RMT_CLK_DIV      100    /*!< RMT counter clock divider */
#define RMT_TICK_10_US    (80000000/RMT_CLK_DIV/100000)   /*!< RMT counter value for 10 us.(Source clock is APB clock) */
#define RMT_TIMEOUT_US  9500   /*!< RMT receiver timeout value(us) */

#define NEC_HEADER_HIGH_US    9000                         /*!< NEC protocol header: positive 9ms */
#define NEC_HEADER_LOW_US     4500                         /*!< NEC protocol header: negative 4.5ms*/
#define NEC_BIT_ONE_HIGH_US    560                         /*!< NEC protocol data bit 1: positive 0.56ms */
#define NEC_BIT_ONE_LOW_US    (2250-NEC_BIT_ONE_HIGH_US)   /*!< NEC protocol data bit 1: negative 1.69ms */
#define NEC_BIT_ZERO_HIGH_US   560                         /*!< NEC protocol data bit 0: positive 0.56ms */
#define NEC_BIT_ZERO_LOW_US   (1120-NEC_BIT_ZERO_HIGH_US)  /*!< NEC protocol data bit 0: negative 0.56ms */
#define NEC_BIT_END            560                         /*!< NEC protocol end: positive 0.56ms */
#define NEC_BIT_MARGIN         20                          /*!< NEC parse margin time */

typedef enum {
    UNKNOWN,
    HEADER,
    ZERO,
    ONE,
    REPEAT,
} item_type_t;

typedef struct {
    rmt_channel_t channel;
    gpio_num_t gpio_num;
    RingbufHandle_t rb;
} ysw_remote_t;

static item_type_t get_item_type(rmt_item32_t *item, int index)
{
#if 0
    ESP_LOGI(TAG, "item[%d].duration0=%d, level0=%d, duration1=%d, level1=%d",
            index,
            item->duration0,
            item->level0,
            item->duration1,
            item->level1);
#endif
    item_type_t type = UNKNOWN;
    if (item->level0 == 0 && item->level1 == 1) {
        if ((7000 < item->duration0 && item->duration0 < 8000) &&
                (3000 < item->duration1 && item->duration1 < 4000)) {
            type = HEADER;
        } else if ((400 < item->duration0 && item->duration0 < 550) &&
                (400 < item->duration1 && item->duration1 < 550)) {
            type = ZERO;
        } else if ((400 < item->duration0 && item->duration0 < 550) &&
                (1000 < item->duration1 && item->duration1 < 2000)) {
            type = ONE;
        } else if ((7000 < item->duration0 && item->duration0 < 8000) &&
                (1000 < item->duration1 && item->duration1 < 2000)) {
            type = REPEAT;
        }
    }
    return type;
}

#define MAX_PACKET_SZ (1 + 16 + 16)

static void process_items(rmt_item32_t *item, size_t rx_size)
{
    int bit = 0;
    uint32_t bits = 0;

    if (rx_size > MAX_PACKET_SZ) {
        rx_size = MAX_PACKET_SZ;
    }

    for (int i = 0; i < rx_size; i++) {
        item_type_t type = get_item_type(&item[i], i);
        switch (type) {
            case UNKNOWN:
                //ESP_LOGI(TAG, "unknown");
                break;
            case HEADER:
                ESP_LOGI(TAG, "header");
                bit = 0;
                bits = 0;
                break;
            case ZERO:
                //ESP_LOGI(TAG, "zero");
                bit++;
                break;
            case ONE:
                //ESP_LOGI(TAG, "one");
                bits |= 1 << bit++;
                break;
            case REPEAT:
                ESP_LOGI(TAG, "repeat");
                bit = 0;
                bits = 0;
                break;
        }
        if (bit == 32) {
            uint8_t data_1 = (bits >> 24) & 0xff;
            uint8_t data_2 = (bits >> 16) & 0xff;
            uint8_t data_3 = (bits >> 8) & 0xff;
            uint8_t data_4 = bits & 0xff;

            ESP_LOGD(TAG, "data_1=%#x, data_2=%#x, data_3=%#x, data_4=%#x", data_1, data_2, data_3, data_4);

            if (data_1 == data_2 && data_3 == (uint8_t) ~data_4) {
                uint16_t data = (data_4 << 8) | (data_2);
                ESP_LOGI(TAG, "custom data=%d (%#x)", data, data);
            } else if (data_1 == (uint8_t) ~data_2 && data_3 == (uint8_t) ~data_4) {
                uint16_t data = (data_4 << 8) | (data_2);
                ESP_LOGI(TAG, "data=%d (%#x)", data, data);
            }

            bit = 0;
            bits = 0;
        }
    }
}

static void handle_ring_buffer(void *context)
{
    ESP_LOGD(TAG, "handle_ring_buffer entered");

    ysw_remote_t *remote = context;

    for (;;) {
        size_t rx_size = 0;
        rmt_item32_t* items = (rmt_item32_t *)xRingbufferReceive(remote->rb, &rx_size, portMAX_DELAY);
        if (items) {
            process_items(items, rx_size);
            vRingbufferReturnItem(remote->rb, items);
        }
    }
}

void ysw_remote_create_task(ysw_bus_t *bus, rmt_channel_t channel, gpio_num_t gpio_num)
{
    ESP_LOGI(TAG, "ysw_remote_create_task channel=%d, gpio_num=%d", channel, gpio_num);

    ysw_remote_t *remote = ysw_heap_allocate(sizeof(ysw_remote_t));

    rmt_config_t rmt_conf = {
        .channel = channel,
        .gpio_num = gpio_num,
        .clk_div = RMT_CLK_DIV,
        .mem_block_num = 1,
        .rmt_mode = RMT_MODE_RX,
        .rx_config.filter_en = true,
        .rx_config.filter_ticks_thresh = 100,
        .rx_config.idle_threshold = RMT_TIMEOUT_US / 10 * (RMT_TICK_10_US),
    };

    $(rmt_config(&rmt_conf));
    $(rmt_driver_install(channel, 1000, 0));
    $(rmt_get_ringbuf_handle(channel, &remote->rb));
    $(rmt_rx_start(channel, 1));

    ysw_task_config_t task_conf = ysw_task_default_config;
    task_conf.name = TAG;
    task_conf.bus = bus;
    task_conf.function = handle_ring_buffer;
    task_conf.context = remote;

    ysw_task_create(&task_conf);
}

