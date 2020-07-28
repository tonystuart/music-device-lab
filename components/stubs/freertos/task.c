#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "task.h"

static int current_timestamp()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    long long milliseconds = t.tv_sec * 1000LL + t.tv_usec / 1000;
    return milliseconds;
}

int xTaskGetTickCount()
{
    static int boot_time;
    if (!boot_time) {
        boot_time = current_timestamp();
    }
    return current_timestamp() - boot_time;
}

void vTaskDelay(int ticks)
{
    fprintf(stderr, "vTaskDelta stub entered, ticks=%d\n", ticks);
}

char *pcTaskGetTaskName(TaskHandle_t handle)
{
    return "UI";
}
