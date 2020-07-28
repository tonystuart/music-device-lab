#include "esp_log.h"
#include "freertos/task.h"
#include "stdarg.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"

typedef enum {
    D,
    I,
    W,
    E
} level_t;

static const char *level_names = "DIWE";

typedef struct {
    char *tag;
    level_t level;
} tag_level_t;

static const tag_level_t tag_levels[] = {
        { "YSW_HEAP", I },
        { "YSW_ARRAY", I }
};

#define TAG_LEVEL_SZ (sizeof(tag_levels) / sizeof(tag_level_t))

static bool is_wanted(const char *tag, int8_t level)
{
    for (int i = 0; i < TAG_LEVEL_SZ; i++) {
        if (strcmp(tag_levels[i].tag, tag) == 0) {
            return level >= tag_levels[i].level;
        }
    }
    return true;
}

void esp_log(uint8_t level, const char *tag, const char *template, ...)
{
    if (level > E) {
        level = E;
    }
    if (is_wanted(tag, level)) {
        va_list arguments;
        va_start(arguments, template);
        FILE *file = level == E ? stderr : stdout;
        fprintf(file, "%c (%d) %s: ", level_names[level], xTaskGetTickCount(), tag);
        vfprintf(file, template, arguments);
        fprintf(file, "\n");
        va_end(arguments);
    }
}
