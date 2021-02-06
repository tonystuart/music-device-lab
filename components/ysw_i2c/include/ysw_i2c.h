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

typedef enum {
    YSW_I2C_SHARED,
    YSW_I2C_EXCLUSIVE,
} ysw_i2c_access_t;

typedef struct {
    i2c_port_t port;
    xSemaphoreHandle mutex;
} ysw_i2c_t;

ysw_i2c_t *ysw_i2c_create(i2c_port_t port, i2c_config_t *config, ysw_i2c_access_t access);
void ysw_i2c_read_reg(ysw_i2c_t *i2c, uint8_t addr, uint8_t reg, uint8_t *value, size_t value_len);
void ysw_i2c_write_reg(ysw_i2c_t *i2c, uint8_t addr, uint8_t reg, uint8_t value);
void ysw_i2c_read_event(ysw_i2c_t *i2c, uint8_t addr, uint8_t *buf);
void ysw_i2c_free(ysw_i2c_t *i2c);
