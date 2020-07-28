
#include "event_groups.h"
#include "ysw_heap.h"
#include "esp_log.h"
#include "assert.h"
#include "stdbool.h"
#include "stdio.h"

#define TAG "EVENT_GROUPS"

static inline bool bits_ready(
        const EventGroupHandle_t xEventGroup,
        const EventBits_t uxBitsToWaitFor,
        const BaseType_t xWaitForAllBits)
{
    uint32_t current_bits_set = uxBitsToWaitFor & xEventGroup->bits;
    return (!xWaitForAllBits && current_bits_set) || (current_bits_set == uxBitsToWaitFor);
}

EventGroupHandle_t xEventGroupCreate()
{
    ESP_LOGD(TAG, "xEventGroupCreate");
    EventGroupHandle_t xEventGroup = ysw_heap_allocate(sizeof(EventGroup_t));
    pthread_mutex_init(&xEventGroup->mutex, NULL);
    pthread_cond_init(&xEventGroup->bits_ready, NULL);
    return xEventGroup;
}

EventBits_t xEventGroupWaitBits(
        const EventGroupHandle_t xEventGroup,
        const EventBits_t uxBitsToWaitFor,
        const BaseType_t xClearOnExit,
        const BaseType_t xWaitForAllBits,
        TickType_t xTicksToWait)
{
    ESP_LOGD(TAG, "xEventGroupWaitBits");
    assert(xEventGroup);
    assert(uxBitsToWaitFor);
    assert(xTicksToWait == portMAX_DELAY); // TODO: implement xTicksToWait
    pthread_mutex_lock(&xEventGroup->mutex);
    while (!bits_ready(xEventGroup, uxBitsToWaitFor, xWaitForAllBits)) {
        pthread_cond_wait(&xEventGroup->bits_ready, &xEventGroup->mutex);
    }
    if (xClearOnExit) {
        xEventGroup->bits &= ~uxBitsToWaitFor;
    }
    pthread_mutex_unlock(&xEventGroup->mutex);
    return true;
}

EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup,
                                 const EventBits_t uxBitsToSet)
{
    ESP_LOGD(TAG, "xEventGroupSetBits");
    assert(xEventGroup);
    pthread_mutex_lock(&xEventGroup->mutex);
    xEventGroup->bits |= uxBitsToSet;
    pthread_cond_signal(&xEventGroup->bits_ready);
    pthread_mutex_unlock(&xEventGroup->mutex);
    return xEventGroup->bits;
}

void vEventGroupDelete(EventGroupHandle_t xEventGroup)
{
    ESP_LOGD(TAG, "xEventGroupDelete");
    pthread_mutex_destroy(&xEventGroup->mutex);
    pthread_cond_destroy(&xEventGroup->bits_ready);
    ysw_heap_free(xEventGroup);
}
