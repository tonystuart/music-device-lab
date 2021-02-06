// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

// Derived from Limor Fried's BSD licensed Adafruit_MPR121.cpp

#include "ysw_mpr121.h"
#include "ysw_i2c.h"

#include "esp_log.h"

#define TAG "YSW_MPR121"

enum {
    MPR121_TOUCHSTATUS_L = 0x00,
    MPR121_TOUCHSTATUS_H = 0x01,
    MPR121_FILTDATA_0L = 0x04,
    MPR121_FILTDATA_0H = 0x05,
    MPR121_BASELINE_0 = 0x1E,
    MPR121_MHDR = 0x2B,
    MPR121_NHDR = 0x2C,
    MPR121_NCLR = 0x2D,
    MPR121_FDLR = 0x2E,
    MPR121_MHDF = 0x2F,
    MPR121_NHDF = 0x30,
    MPR121_NCLF = 0x31,
    MPR121_FDLF = 0x32,
    MPR121_NHDT = 0x33,
    MPR121_NCLT = 0x34,
    MPR121_FDLT = 0x35,

    MPR121_TOUCHTH_0 = 0x41,
    MPR121_RELEASETH_0 = 0x42,
    MPR121_DEBOUNCE = 0x5B,
    MPR121_CONFIG1 = 0x5C,
    MPR121_CONFIG2 = 0x5D,
    MPR121_CHARGECURR_0 = 0x5F,
    MPR121_CHARGETIME_1 = 0x6C,
    MPR121_ECR = 0x5E,
    MPR121_AUTOCONFIG0 = 0x7B,
    MPR121_AUTOCONFIG1 = 0x7C,
    MPR121_UPLIMIT = 0x7D,
    MPR121_LOWLIMIT = 0x7E,
    MPR121_TARGETLIMIT = 0x7F,

    MPR121_GPIODIR = 0x76,
    MPR121_GPIOEN = 0x77,
    MPR121_GPIOSET = 0x78,
    MPR121_GPIOCLR = 0x79,
    MPR121_GPIOTOGGLE = 0x7A,

    MPR121_SOFTRESET = 0x80,
};

static void write_register(ysw_i2c_t *i2c, uint8_t address, uint8_t reg, uint8_t value) {
    ysw_i2c_write_reg(i2c, address, reg, value);
}

static uint8_t read_uint8(ysw_i2c_t *i2c, uint8_t address, uint8_t reg) {
    uint8_t value;
    ysw_i2c_read_reg(i2c, address, reg, &value, 1);
    return value;
}

static uint16_t read_uint16(ysw_i2c_t *i2c, uint8_t address, uint8_t reg)
{
    uint16_t value;
    ysw_i2c_read_reg(i2c, address, reg, (uint8_t *)&value, 2);
    // TODO: Consider endianess
    return value;
}

void ysw_mpr121_initialize(ysw_i2c_t *i2c, uint8_t address)
{
    write_register(i2c, address, MPR121_SOFTRESET, 0x63);
    write_register(i2c, address, MPR121_ECR, 0x0);
    uint8_t c = read_uint8(i2c, address, MPR121_CONFIG2);
    if (c != 0x24) {
        ESP_LOGE(TAG, "read_uint8(MPR121_CONFIG2) failed");
        abort();
    }

    ysw_mpr121_set_thresholds(i2c, address, 12, 6);
    write_register(i2c, address, MPR121_MHDR, 0x01);
    write_register(i2c, address, MPR121_NHDR, 0x01);
    write_register(i2c, address, MPR121_NCLR, 0x0E);
    write_register(i2c, address, MPR121_FDLR, 0x00);

    write_register(i2c, address, MPR121_MHDF, 0x01);
    write_register(i2c, address, MPR121_NHDF, 0x05);
    write_register(i2c, address, MPR121_NCLF, 0x01);
    write_register(i2c, address, MPR121_FDLF, 0x00);

    write_register(i2c, address, MPR121_NHDT, 0x00);
    write_register(i2c, address, MPR121_NCLT, 0x00);
    write_register(i2c, address, MPR121_FDLT, 0x00);

    write_register(i2c, address, MPR121_DEBOUNCE, 0);
    write_register(i2c, address, MPR121_CONFIG1, 0x10); // default, 16uA charge current
    write_register(i2c, address, MPR121_CONFIG2, 0x20); // 0.5uS encoding, 1ms period

    write_register(i2c, address, MPR121_ECR, 0x8F); // start with first 5 bits of baseline tracking
}

/*!
 *  @brief      Set the touch and release thresholds for all 13 channels on the
 *              device to the passed values. The threshold is defined as a
 *              deviation value from the baseline value, so it remains constant even baseline
 *              value changes. Typically the touch threshold is a little bigger than the
 *              release threshold to touch debounce and hysteresis. For typical touch
 *              application, the value can be in range 0x05~0x30 for example. The setting of
 *              the threshold is depended on the actual application. For the operation details
 *              and how to set the threshold refer to application note AN3892 and MPR121
 *              design guidelines.
 *  @param      touch 
 *              the touch threshold value from 0 to 255.
 *  @param      release 
 *              the release threshold from 0 to 255.
 */
void ysw_mpr121_set_thresholds(ysw_i2c_t *i2c, uint8_t address, uint8_t touch, uint8_t release)
{
    for (uint8_t i = 0; i < 12; i++) {
        write_register(i2c, address, MPR121_TOUCHTH_0 + 2 * i, touch);
        write_register(i2c, address, MPR121_RELEASETH_0 + 2 * i, release);
    }
}

/*!
 *  @brief      Read the filtered data from channel t. The ADC raw data outputs
 *              run through 3 levels of digital filtering to filter out the high frequency and
 *              low frequency noise encountered. For detailed information on this filtering
 *              see page 6 of the device datasheet.
 *  @param      t
 *              the channel to read
 *  @returns    the filtered reading as a 10 bit unsigned value
 */
uint16_t ysw_mpr121_get_filtered_data(ysw_i2c_t *i2c, uint8_t address, uint8_t t)
{
    assert(t <= 12);
    return read_uint16(i2c, address, MPR121_FILTDATA_0L + t * 2);
}

/*!
 *  @brief      Read the baseline value for the channel. The 3rd level filtered
 *              result is internally 10bit but only high 8 bits are readable from registers
 *              0x1E~0x2A as the baseline value output for each channel.
 *  @param      t 
 *              the channel to read.
 *  @returns    the baseline data that was read
 */
uint16_t ysw_mpr121_get_baseline_data(ysw_i2c_t *i2c, uint8_t address, uint8_t t)
{
    assert(t <= 12);
    uint16_t bl = read_uint8(i2c, address, MPR121_BASELINE_0 + t);
    return (bl << 2);
}

/**
 *  @brief      Read the touch status of all 13 channels as bit values in a 12 bit integer.
 *  @returns    a 12 bit integer with each bit corresponding to the touch status
 *              of a sensor. For example, if bit 0 is set then channel 0 of the device is
 *              currently deemed to be touched.
 */
uint16_t ysw_mpr121_get_touches(ysw_i2c_t *i2c, uint8_t address)
{
    uint16_t t = read_uint16(i2c, address, MPR121_TOUCHSTATUS_L);
    return t & 0x0FFF;
}

