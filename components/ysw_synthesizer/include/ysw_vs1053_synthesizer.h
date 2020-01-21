// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "ysw_task.h"
#include "ysw_vs1053_driver.h"
#include "ysw_synthesizer.h"

QueueHandle_t ysw_vs1053_synthesizer_create_task(ysw_vs1053_synthesizer_config_t *vs1053_config);
