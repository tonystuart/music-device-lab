// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_i2c.h"
#include "stdint.h"

void ysw_mpr121_initialize(ysw_i2c_t *i2c, uint8_t address);
void ysw_mpr121_set_thresholds(ysw_i2c_t *i2c, uint8_t address, uint8_t touch, uint8_t release);
uint16_t ysw_mpr121_get_filtered_data(ysw_i2c_t *i2c, uint8_t address, uint8_t t);
uint16_t ysw_mpr121_get_baseline_data(ysw_i2c_t *i2c, uint8_t address, uint8_t t);
uint16_t ysw_mpr121_get_touches(ysw_i2c_t *i2c, uint8_t address);
