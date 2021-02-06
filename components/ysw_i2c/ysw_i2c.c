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
#include "ysw_heap.h"
#include "ysw_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

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

ysw_i2c_t *ysw_i2c_create(i2c_port_t port, i2c_config_t *config, ysw_i2c_access_t access)
{
    ESP_LOGI(TAG, "ysw_i2c_init port=%d sda=%d, scl=%d, clk=%d, access=%d",
            port, config->sda_io_num, config->scl_io_num, config->master.clk_speed, access);

    ysw_i2c_t *i2c = ysw_heap_allocate(sizeof(ysw_i2c_t));
    i2c->port = port;

    if (access == YSW_I2C_SHARED) {
        $$(i2c->mutex = xSemaphoreCreateMutex());
    }

	$(i2c_param_config(port, config));
	$(i2c_driver_install(port, config->mode, 0, 0, 0));

    return i2c;
}

void ysw_i2c_read_reg(ysw_i2c_t *i2c, uint8_t addr, uint8_t reg, uint8_t *value, size_t value_len)
{
    if (i2c->mutex) {
        $$(xSemaphoreTake(i2c->mutex, portMAX_DELAY) == pdTRUE);
    }

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    assert(cmd);

	$(i2c_master_start(cmd));
	$(i2c_master_write_byte(cmd, ( addr << 1 ) | I2C_MASTER_WRITE, ACK_CHECK_EN));
	$(i2c_master_write_byte(cmd, reg, ACK_CHECK_EN));

	$(i2c_master_start(cmd));
	$(i2c_master_write_byte(cmd, ( addr << 1 ) | I2C_MASTER_READ, ACK_CHECK_EN));
	if (value_len > 1)
	{
		$(i2c_master_read(cmd, value, value_len-1, ACK_VAL));
	}
	$(i2c_master_read_byte(cmd, &value[value_len-1], NACK_VAL));
	$(i2c_master_stop(cmd));

	$(i2c_master_cmd_begin(i2c->port, cmd, 1000 / portTICK_RATE_MS));
	i2c_cmd_link_delete(cmd);

    if (i2c->mutex) {
        $$(xSemaphoreGive(i2c->mutex) == pdTRUE);
    }
}

void ysw_i2c_write_reg(ysw_i2c_t *i2c, uint8_t addr, uint8_t reg, uint8_t value)
{
    if (i2c->mutex) {
        $$(xSemaphoreTake(i2c->mutex, portMAX_DELAY) == pdTRUE);
    }

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    assert(cmd);

	$(i2c_master_start(cmd));
	$(i2c_master_write_byte(cmd, ( addr << 1 ) | I2C_MASTER_WRITE, ACK_CHECK_EN));
	$(i2c_master_write_byte(cmd, reg, ACK_CHECK_EN));
	$(i2c_master_write_byte(cmd, value, ACK_CHECK_EN));
	$(i2c_master_stop(cmd));

	$(i2c_master_cmd_begin(i2c->port, cmd, 1000 / portTICK_RATE_MS));
	i2c_cmd_link_delete(cmd);

    if (i2c->mutex) {
        $$(xSemaphoreGive(i2c->mutex) == pdTRUE);
    }
}

void ysw_i2c_read_event(ysw_i2c_t *i2c, uint8_t addr, uint8_t *buf)
{
    if (i2c->mutex) {
        $$(xSemaphoreTake(i2c->mutex, portMAX_DELAY) == pdTRUE);
    }

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    assert(cmd);

	$(i2c_master_start(cmd));
	$(i2c_master_write_byte(cmd, ( addr << 1 ) | I2C_MASTER_READ, ACK_CHECK_EN));
	$(i2c_master_read(cmd, buf, 2, ACK_VAL));
	$(i2c_master_read_byte(cmd, &buf[2], NACK_VAL));
	$(i2c_master_stop(cmd));

	$(i2c_master_cmd_begin(i2c->port, cmd, 1000 / portTICK_RATE_MS));
	i2c_cmd_link_delete(cmd);

    if (i2c->mutex) {
        $$(xSemaphoreGive(i2c->mutex) == pdTRUE);
    }
}

void ysw_i2c_free(ysw_i2c_t *i2c)
{
    if (i2c->mutex) {
        vSemaphoreDelete(i2c->mutex);
    }
    ysw_heap_free(i2c);
}
