// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_common.h"
#include "ysw_heap.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "stdio.h"

uint32_t ysw_rtos_ticks_to_millis(uint32_t ticks)
{
    return ticks * portTICK_PERIOD_MS;
}

uint32_t ysw_millis_to_rtos_ticks(uint32_t millis)
{
    return millis / portTICK_PERIOD_MS;
}

uint32_t ysw_get_millis()
{
    return ysw_rtos_ticks_to_millis(xTaskGetTickCount());
}

void ysw_wait_millis(int millis)
{
    vTaskDelay(ysw_millis_to_rtos_ticks(millis));
}

char *ysw_itoa(int input_value, char *buffer, int buffer_size)
{
    return snprintf(buffer, buffer_size, "%d", input_value) < buffer_size ? buffer : "";
}

void ysw_copy(char *destination, const char* source, size_t size)
{
    strncpy(destination, source, size);
    destination[size - 1] = EOS;
}

char *ysw_replace(const char *source, const char *from, const char *to)
{
    int match_count = 0;
    int from_length = strlen(from);
    int to_length = strlen(to);
    const char *source_p = source;
    while (*source_p) {
        const char *match_p = source_p;
        const char *from_p = from;
        bool match = true;
        while (*match_p && *from_p && match) {
            if (*match_p++ != *from_p++) {
                match = false;
            }
        }
        if (match) {
            match_count++;
            source_p = match_p;
        }
        source_p++;
    }
    source_p++; // count null terminator
    int old_length = source_p - source;
    int delta = to_length - from_length;
    int new_length = old_length + (match_count * delta);
    char *target = ysw_heap_allocate(new_length);
    char *target_p = target;
    source_p = source;
    while (*source_p) {
        const char *match_p = source_p;
        const char *from_p = from;
        bool match = true;
        while (*match_p && *from_p && match) {
            if (*match_p++ != *from_p++) {
                match = false;
            }
        }
        if (match) {
            const char *to_p = to;
            while (*to_p) {
                *target_p++ = *to_p++;
            }
            source_p = match_p;
        } else {
            *target_p++ = *source_p++;
        }
    }
    *target_p = 0;
    return target;
}

