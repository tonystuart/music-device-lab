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

#define YSW_R1_C1 5
#define YSW_R1_C2 6
#define YSW_R1_C3 7
#define YSW_R1_C4 8

#define YSW_R2_C1 16
#define YSW_R2_C2 17
#define YSW_R2_C3 18
#define YSW_R2_C4 19

#define YSW_R3_C1 25
#define YSW_R3_C2 26
#define YSW_R3_C3 27
#define YSW_R3_C4 28

#define YSW_R4_C1 36
#define YSW_R4_C2 37
#define YSW_R4_C3 38
#define YSW_R4_C4 39

extern const ysw_menu_softkey_t ysw_app_softkey_map[];

typedef enum {
    YSW_APP_CONTINUE,
    YSW_APP_TERMINATE,
} ysw_app_control_t;

typedef ysw_app_control_t (*ysw_app_event_handler_t)(void *context, ysw_event_t *event);

QueueHandle_t ysw_app_create_queue();

void ysw_app_handle_events(QueueHandle_t queue, ysw_app_event_handler_t process_event, void *context);
