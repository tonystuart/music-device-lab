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
#include "lvgl.h"

#define YSW_MENU_NOP 0x00
#define YSW_MENU_DOWN 0x01
#define YSW_MENU_UP 0x02
#define YSW_MENU_PRESS 0x04

#define YSW_MENU_POP_ALL 0x10
#define YSW_MENU_POP_TOP 0x20
#define YSW_MENU_LINE 0x40
#define YSW_MENU_LEGEND 0x80

typedef struct ysw_menu_s ysw_menu_t;

typedef void (*ysw_menu_cb_t)(ysw_menu_t *menu, ysw_event_t *event, void *caller_context, void *value);

typedef struct {
    const char *name;
    const uint32_t flags;
    ysw_menu_cb_t cb;
    void *value;
} ysw_menu_item_t;

typedef struct ysw_menu_s {
    const ysw_menu_item_t *menu_items;
    ysw_array_t *menu_stack;
    lv_obj_t *container;
} ysw_menu_t;

void ysw_menu_on_key_down(ysw_menu_t *menu, ysw_event_t *event, void *caller_context);

void ysw_menu_on_key_up(ysw_menu_t *menu, ysw_event_t *event, void *caller_context);

void ysw_menu_on_key_pressed(ysw_menu_t *menu, ysw_event_t *event, void *caller_context);

void ysw_menu_on_open(ysw_menu_t *menu, ysw_event_t *event, void *caller_context, void *value);

void ysw_menu_on_close(ysw_menu_t *menu, ysw_event_t *event, void *caller_context, void *value);

ysw_menu_t *ysw_menu_create(const ysw_menu_item_t *menu_items);

void ysw_menu_nop(ysw_menu_t *menu, ysw_event_t *event, void *caller_context, void *value);
