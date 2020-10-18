// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// Derived from Chris Osborn's public domain ws2812.h

#ifndef WS2812_DRIVER_H
#define WS2812_DRIVER_H

#include <stdint.h>

typedef union {
  struct __attribute__ ((packed)) {
    uint8_t r, g, b;
  };
  uint32_t num;
} ws2812_color_t;

#define ws2812_color(r1, g1, b1) { .r = r1, .g = g1, .b = b1 }

extern void ws2812_initialize(int gpio, int length, uint8_t max_power);
extern void ws2812_set_color(int led, ws2812_color_t color);
extern void ws2812_update_display(void);

static inline ws2812_color_t ws2812_make_color(uint8_t r, uint8_t g, uint8_t b)
{
  return (ws2812_color_t){ .r = r, .g = g, .b = b, };
}

#endif /* WS2812_DRIVER_H */
