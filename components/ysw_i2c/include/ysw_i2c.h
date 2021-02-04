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

void ysw_i2c_init(i2c_port_t port, gpio_num_t sda, gpio_num_t scl);
void ysw_i2c_read_reg(i2c_port_t port, uint8_t addr, uint8_t reg, uint8_t *value, size_t value_len);
void ysw_i2c_write_reg(i2c_port_t port, uint8_t addr, uint8_t reg, uint8_t value);
void ysw_i2c_read_event(i2c_port_t port, uint8_t addr, uint8_t *buf);

