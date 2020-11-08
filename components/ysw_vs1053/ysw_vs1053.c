// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_vs1053.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "ysw_common.h"

// http://www.vlsi.fi/fileadmin/software/VS10XX/rtmidi.pdf
// Section 1.2 Loading Through SCI

#include "rtmidi1053b.plg"

#define TAG "YSW_VS1053"

#define $ ESP_ERROR_CHECK

#define LOW 0
#define HIGH 1

#define WRITE 0b00000010
#define READ 0b00000011

// http://www.vlsi.fi/fileadmin/datasheets/vs1053.pdf
// Section 8.7 SCI Registers

#define SCI_MODE 0x0
#define SCI_STATUS 0x1
#define SCI_BASS 0x2
#define SCI_CLOCKF 0x3
#define SCI_DECODE_TIME 0x4
#define SCI_AUDATA 0x5
#define SCI_WRAM 0x6
#define SCI_WRAMADDR 0x7
#define SCI_HDAT0 0x8
#define SCI_HDAT1 0x9
#define SCI_AIADDR 0xa
#define SCI_VOL 0xb
#define SCI_AICTRL0 0xc
#define SCI_AICTRL1 0xd
#define SCI_AICTRL2 0xe
#define SCI_AICTRL3 0xf

// http://www.vlsi.fi/fileadmin/datasheets/vs1053.pdf
// Section 8.7.1 SCI_MODE (RW)

#define SM_DIFF 0
#define SM_LAYER12 1
#define SM_RESET 2
#define SM_CANCEL 3
#define SM_EARSPEAKER_LO 4
#define SM_TESTS 5
#define SM_STREAM 6
#define SM_EARSPEAKER_HI 7
#define SM_DACT 8
#define SM_SDIORD 9
#define SM_SDISHARE 10
#define SM_SDINEW 11
#define SM_ADPCM 12
#define SM_UNUSED 13
#define SM_LINE1 14
#define SM_CLK_RANGE 15

// http://www.vlsi.fi/fileadmin/datasheets/vs1053.pdf
// Section 8.7.4 SCI_CLOCKF (RW)

#define XTALI_X_1_0 0x0000
#define XTALI_X_2_0 0x2000
#define XTALI_X_2_5 0x4000
#define XTALI_X_3_0 0x6000
#define XTALI_X_3_5 0x8000
#define XTALI_X_4_0 0xa000
#define XTALI_X_4_5 0xc000
#define XTALI_X_5_0 0xe000

// http://www.vlsi.fi/fileadmin/datasheets/vs1053.pdf
// Section 10.11 Extra Parameters and Section 10.11.4 Midi

#define EP_VERSION 0x1e02
#define EP_CONFIG1 0x1e03

#define REVERB_OFF 0x0001

// http://www.vlsi.fi/fileadmin/software/VS10XX/rtmidi.pdf
// Section 1.1 Boot Images

#define MIDI_CLOCKF XTALI_X_4_5
#define MIDI_VOL 0x3030

// http://www.vlsi.fi/fileadmin/software/VS10XX/rtmidi.pdf
// Section 1.2 Loading Through SCI

#define MIDI_START 0x50

// https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/spi_master.html

static spi_device_handle_t control_spi;
static spi_device_handle_t data_spi;

static int32_t dreq_gpio = -1;

static void wait_for_dreq()
{
    if (dreq_gpio != -1) {
        while (gpio_get_level(dreq_gpio) == LOW) {
        }
    }
}

static void write_register(uint8_t address, uint16_t value)
{
    wait_for_dreq();
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA,
        .tx_data[0] = WRITE,
        .tx_data[1] = address,
        .tx_data[2] = value >> 8,
        .tx_data[3] = value & 0xff,
        .length = 4 * 8, // bits
    };
    $(spi_device_transmit(control_spi, &t));
}

static uint16_t read_register(uint8_t address)
{
    wait_for_dreq();
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA|SPI_TRANS_USE_RXDATA,
        .tx_data[0] = READ,
        .tx_data[1] = address,
        .tx_data[2] = 0xff,
        .tx_data[3] = 0xff,
        .length = 4 * 8, // bits
    };
    $(spi_device_transmit(control_spi, &t));
    uint16_t value = (((uint8_t*)t.rx_data)[2] << 8) | (((uint8_t*)t.rx_data)[3] & 0xff);
    return value;
}

static uint16_t read_ram(uint16_t address)
{
    write_register(SCI_WRAMADDR, address);
    uint16_t value = read_register(SCI_WRAM);
    return value;
}

static UNUSED void write_ram(uint16_t address, uint16_t value)
{
    write_register(SCI_WRAMADDR, address);
    write_register(address, value);
}

static void write_midi(uint8_t a, uint8_t b, uint8_t c)
{
    wait_for_dreq();
    uint8_t buffer[6] = {
        [1] = a,
        [3] = b,
        [5] = c,
    };
    spi_transaction_t t = {
        .tx_buffer = buffer,
        .length = 6 * 8, // bits
    };
    $(spi_device_transmit(data_spi, &t));
}

// http://www.vlsi.fi/fileadmin/datasheets/vs1053.pdf
// Section 9.10 Real-Time MIDI

// http://www.vlsi.fi/fileadmin/software/VS10XX/rtmidi.pdf
// Section 2 How to Load a Plugin

// The vector is decoded as follows:
// 1. Read register address number addr and repeat number n.
// 2. If (n & 0x8000U), write the next word n times to register addr.
// 3. Else write next n words to register addr.
// 4. Continue until array has been exhausted.

static void load_patch(const uint16_t plugin[], int32_t length)
{
    ESP_LOGD(TAG, "load_patch length=%d", length);
    int i = 0;
    while (i<length) {
        unsigned short addr, n, val;
        addr = plugin[i++];
        n = plugin[i++];
        if (n & 0x8000U) { /* RLE run, replicate n samples */
            n &= 0x7FFF;
            val = plugin[i++];
            while (n--) {
                write_register(addr, val);
            }
        } else {           /* Copy run, copy n samples */
            while (n--) {
                val = plugin[i++];
                write_register(addr, val);
            }
        }
    }
}

void ysw_vs1053_set_note_on(uint8_t channel, uint8_t midi_note, uint8_t velocity)
{
    write_midi(channel | 0x90, midi_note, velocity);
}

void ysw_vs1053_set_note_off(uint8_t channel, uint8_t midi_note)
{
    write_midi(channel | 0x80, midi_note, 0);
}

void ysw_vs1053_select_program(uint8_t channel, uint8_t program)
{
    write_midi(channel | 0xc0, 0, program);
}

void ysw_vs1053_initialize(ysw_vs1053_config_t *vs1053_config)
{
    dreq_gpio = vs1053_config->dreq_gpio;
    if (dreq_gpio != -1) {
        gpio_set_direction(vs1053_config->dreq_gpio, GPIO_MODE_INPUT);
    }
    gpio_set_direction(vs1053_config->xrst_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(vs1053_config->xrst_gpio, LOW);

    spi_bus_config_t buscfg = {
        .miso_io_num = vs1053_config->miso_gpio,
        .mosi_io_num = vs1053_config->mosi_gpio,
        .sclk_io_num = vs1053_config->sck_gpio,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32*8,
    };
    $(spi_bus_initialize(vs1053_config->spi_host, &buscfg, 0));

    spi_device_interface_config_t control_cfg = {
        .clock_speed_hz = 1*1000*1000,
        .mode = 0,
        .spics_io_num = vs1053_config->xcs_gpio,
        .queue_size = 16,
    };
    $(spi_bus_add_device(vs1053_config->spi_host, &control_cfg, &control_spi));

    spi_device_interface_config_t data_cfg = {
        .clock_speed_hz = 1*1000*1000,
        .mode = 0,
        .spics_io_num = vs1053_config->xdcs_gpio,
        .queue_size = 16,
    };
    $(spi_bus_add_device(vs1053_config->spi_host, &data_cfg, &data_spi));

    gpio_set_level(vs1053_config->xrst_gpio, HIGH);

    write_register(SCI_MODE, BIT(SM_RESET) | BIT(SM_SDINEW));
    write_register(SCI_CLOCKF, MIDI_CLOCKF);
    load_patch(plugin, PLUGIN_SIZE);
    write_register(SCI_AIADDR, MIDI_START);
    write_register(SCI_MODE, BIT(SM_SDINEW));
    //write_register(SCI_VOL, MIDI_VOL);

    uint16_t version = read_ram(EP_VERSION);
    uint16_t config1 = read_ram(EP_CONFIG1);
    ESP_LOGD(TAG, "version=%#x, config1=%#x", version, config1);

#if 0
    for (int i = 0; i < 16; i++) {
        uint16_t value = read_register(i);
        ESP_LOGD(TAG, "address=%d, value=%#04x", i, value);
    }
#endif
}

