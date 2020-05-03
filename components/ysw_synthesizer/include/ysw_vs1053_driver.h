// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "stdint.h"
#include "driver/spi_master.h"

typedef struct {
    int8_t dreq_gpio;
    uint8_t xrst_gpio;
    uint8_t miso_gpio;
    uint8_t mosi_gpio;
    uint8_t sck_gpio;
    uint8_t xcs_gpio;
    uint8_t xdcs_gpio;
    spi_host_device_t spi_host;
} ysw_vs1053_synthesizer_config_t;

void ysw_vs1053_synthesizer_initialize(ysw_vs1053_synthesizer_config_t *vs1053_config);
void ysw_vs1053_synthesizer_set_note_on(uint8_t channel, uint8_t midi_note, uint8_t velocity);
void ysw_vs1053_synthesizer_set_note_off(uint8_t channel, uint8_t midi_note);
void ysw_vs1053_synthesizer_select_program(uint8_t channel, uint8_t program);
