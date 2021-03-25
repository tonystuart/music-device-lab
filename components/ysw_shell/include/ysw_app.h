// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_event.h"
#include "ysw_menu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
	YSW_R1_C1 = 1, // ysw_mapper interprets a target value of zero as a no-op
	YSW_R1_C2,
	YSW_R1_C3,
	YSW_R1_C4,

	YSW_R2_C1,
	YSW_R2_C2,
	YSW_R2_C3,
	YSW_R2_C4,

	YSW_R3_C1,
	YSW_R3_C2,
	YSW_R3_C3,
	YSW_R3_C4,

	YSW_R4_C1,
	YSW_R4_C2,
	YSW_R4_C3,
	YSW_R4_C4,

    YSW_BUTTON_1,
    YSW_BUTTON_2,
    YSW_BUTTON_3,
    YSW_BUTTON_4,
    YSW_BUTTON_5,
} ysw_app_softkey_t;

extern const ysw_menu_softkey_t ysw_app_softkey_map[];

typedef void (*ysw_app_event_handler_t)(void *context, ysw_event_t *event);

QueueHandle_t ysw_app_create_queue();

void ysw_app_handle_events(QueueHandle_t queue, ysw_app_event_handler_t process_event, void *context);

void ysw_app_terminate();
