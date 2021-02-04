// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// Derived from Bas van Sisseren's Apache 2.0 licensed SHA2017-badge/Firmware/components/badge_i2c.c

#include "ysw_i2c.h"
#include "ysw_common.h"
#include "esp_log.h"

#define I2C_MASTER_FREQ_HZ         100000
//define I2C_MASTER_FREQ_HZ        400000
#define I2C_MASTER_TX_BUF_DISABLE  0
#define I2C_MASTER_RX_BUF_DISABLE  0

#define WRITE_BIT      I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT       I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave */
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL        0x0     /*!< I2C ack value */
#define NACK_VAL       0x1     /*!< I2C nack value */

#define TAG "YSW_I2C"

// See https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html

// ESP32’s internal pull-ups are in the range of tens of kOhm, which is,
// in most cases, insufficient for use as I2C pull-ups. Users are advised
// to use external pull-ups with values described in the I2C specification.

// See https://www.nxp.com/docs/en/user-guide/UM10204.pdf

void ysw_i2c_init(i2c_port_t port, gpio_num_t sda, gpio_num_t scl)
{
	i2c_config_t conf = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = sda,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_io_num = scl,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = I2C_MASTER_FREQ_HZ,
	};

	$(i2c_param_config(port, &conf));
	$(i2c_driver_install(port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0));
}

void ysw_i2c_read_reg(i2c_port_t port, uint8_t addr, uint8_t reg, uint8_t *value, size_t value_len)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    assert(cmd);

	$(i2c_master_start(cmd));
	$(i2c_master_write_byte(cmd, ( addr << 1 ) | WRITE_BIT, ACK_CHECK_EN));
	$(i2c_master_write_byte(cmd, reg, ACK_CHECK_EN));

	$(i2c_master_start(cmd));
	$(i2c_master_write_byte(cmd, ( addr << 1 ) | READ_BIT, ACK_CHECK_EN));
	if (value_len > 1)
	{
		$(i2c_master_read(cmd, value, value_len-1, ACK_VAL));
	}
	$(i2c_master_read_byte(cmd, &value[value_len-1], NACK_VAL));
	$(i2c_master_stop(cmd));

	$(i2c_master_cmd_begin(port, cmd, 1000 / portTICK_RATE_MS));
	i2c_cmd_link_delete(cmd);
}

void ysw_i2c_write_reg(i2c_port_t port, uint8_t addr, uint8_t reg, uint8_t value)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    assert(cmd);

	$(i2c_master_start(cmd));
	$(i2c_master_write_byte(cmd, ( addr << 1 ) | WRITE_BIT, ACK_CHECK_EN));
	$(i2c_master_write_byte(cmd, reg, ACK_CHECK_EN));
	$(i2c_master_write_byte(cmd, value, ACK_CHECK_EN));
	$(i2c_master_stop(cmd));

	$(i2c_master_cmd_begin(port, cmd, 1000 / portTICK_RATE_MS));
	i2c_cmd_link_delete(cmd);
}

void ysw_i2c_read_event(i2c_port_t port, uint8_t addr, uint8_t *buf)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    assert(cmd);

	$(i2c_master_start(cmd));
	$(i2c_master_write_byte(cmd, ( addr << 1 ) | READ_BIT, ACK_CHECK_EN));
	$(i2c_master_read(cmd, buf, 2, ACK_VAL));
	$(i2c_master_read_byte(cmd, &buf[2], NACK_VAL));
	$(i2c_master_stop(cmd));

	$(i2c_master_cmd_begin(port, cmd, 1000 / portTICK_RATE_MS));
	i2c_cmd_link_delete(cmd);
}

