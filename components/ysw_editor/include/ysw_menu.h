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

#define YSW_MENU_POP 0x100
#define YSW_MENU_POP_ALL 0x200

#define YSW_MENU_OPEN 0x1000
#define YSW_MENU_CLOSE 0x2000


typedef enum {
    YSW_MENU_ENDLINE = -1,
    YSW_MENU_ENDGRID = -2,
} ysw_menu_softmap_t;

typedef struct ysw_menu_s ysw_menu_t;

typedef void (*ysw_menu_cb_t)(ysw_menu_t *menu, ysw_event_t *event, void *value);

typedef struct {
    uint32_t scan_code;
    const char *name;
    const uint32_t flags;
    ysw_menu_cb_t cb;
    void *value;
} ysw_menu_item_t;

typedef struct {
    lv_obj_t *container;
    const char **button_map;
} ysw_softkeys_t;

typedef struct ysw_menu_s {
    uint32_t softmap_size; // number of scan codes in softmap
    const ysw_menu_softmap_t *softmap; // scan codes comprising soft keys
    ysw_array_t *stack; // each element is the base of an array of menu items
    ysw_softkeys_t *softkeys; // null if not currently displaying softkeys
    void *context; // arbitrary context passed through from caller
} ysw_menu_t;

void ysw_menu_on_key_down(ysw_menu_t *menu, ysw_event_t *event);

void ysw_menu_on_key_up(ysw_menu_t *menu, ysw_event_t *event);

void ysw_menu_on_key_pressed(ysw_menu_t *menu, ysw_event_t *event);

void ysw_menu_nop(ysw_menu_t *menu, ysw_event_t *event, void *value);

ysw_menu_t *ysw_menu_create(const ysw_menu_item_t *menu_items, const ysw_menu_softmap_t softkeys[], void *context);

void ysw_menu_show(ysw_menu_t *menu);
