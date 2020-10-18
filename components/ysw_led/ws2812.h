// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

/* Created 19 Nov 2016 by Chris Osborn <fozztexx@fozztexx.com>
 * http://insentricity.com
 *
 * This is a driver for the WS2812 RGB LEDs using the RMT peripheral on the ESP32.
 *
 * This code is placed in the public domain (or CC0 licensed, at your option).
 */

#ifndef WS2812_DRIVER_H
#define WS2812_DRIVER_H

#include <stdint.h>

typedef union {
  struct __attribute__ ((packed)) {
    uint8_t r, g, b;
  };
  uint32_t num;
} rgbVal;

extern void ws2812_init(int gpioNum);
extern void ws2812_setColors(unsigned int length, rgbVal *array);

inline rgbVal makeRGBVal(uint8_t r, uint8_t g, uint8_t b)
{
  rgbVal v;


  v.r = r;
  v.g = g;
  v.b = b;
  return v;
}

#endif /* WS2812_DRIVER_H */
