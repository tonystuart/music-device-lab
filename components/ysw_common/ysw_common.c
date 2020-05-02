// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdio.h"

uint32_t to_millis(uint32_t ticks)
{
    return ticks * portTICK_PERIOD_MS;
}

uint32_t to_ticks(uint32_t millis)
{
    return millis / portTICK_PERIOD_MS;
}

uint32_t get_millis()
{
    return to_millis(xTaskGetTickCount());
}

void wait_millis(int millis)
{
    vTaskDelay(to_ticks(millis));
}

char *ysw_itoa(int input_value, char *buffer, int buffer_size)
{
    return snprintf(buffer, buffer_size, "%d", input_value) < buffer_size ? buffer : "";
}

void ysw_copy(char *destination, const char* source, size_t size)
{
    strncpy(destination, source, size);
    destination[size - RFT] = EOS;
}

