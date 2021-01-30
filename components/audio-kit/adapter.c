
#include "ysw_heap.h"
#include "audio_hal.h"
#include "esp_log.h"
#include "stdlib.h"

#define TAG "AUDIO_KIT_STUBS"

// Defined in audio_mem.c

void *audio_calloc(size_t nmemb, size_t size)
{
    return ysw_heap_allocate(nmemb * size);
}

void audio_free(void *ptr)
{
    ysw_heap_free(ptr);
}
