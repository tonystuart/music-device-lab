
#include "esp_system.h"
#include "stdlib.h"
#include "time.h"

uint32_t esp_random()
{
    static unsigned int seed;
    if (!seed) {
        seed = time(NULL);
    }
    return rand_r(&seed);
}
