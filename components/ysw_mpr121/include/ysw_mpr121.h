// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "driver/i2c.h"
#include "stdint.h"

void ysw_mpr121_initialize(i2c_port_t port, uint8_t address);
void ysw_mpr121_set_thresholds(i2c_port_t port, uint8_t address, uint8_t touch, uint8_t release);
uint16_t ysw_mpr121_get_filtered_data(i2c_port_t port, uint8_t address, uint8_t t);
uint16_t ysw_mpr121_get_baseline_data(i2c_port_t port, uint8_t address, uint8_t t);
uint16_t ysw_mpr121_get_touches(i2c_port_t port, uint8_t address);
