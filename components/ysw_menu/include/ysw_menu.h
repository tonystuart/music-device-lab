// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#pragma once

#include "ysw_bus.h"
#include "ysw_event.h"
#include "lvgl.h"

#define YSW_MENU_NOP 0x00

typedef enum {
    YSW_MENU_ROOT_SHOW,
    YSW_MENU_ROOT_HIDE,
} ysw_menu_root_handling_t;

typedef enum {
    YSW_MENU_PLUS = 0x01,
    YSW_MENU_MINUS = 0x02,
    YSW_MENU_SHOW = 0x03,
    YSW_MENU_HIDE = 0x04,
    YSW_MENU_TOGGLE = 0x05,
    YSW_MENU_RESET = 0x06,
} ysw_menu_action_t;

#define YSW_MENU_ACTION_MASK 0x0f

typedef enum {
    YSW_MENU_DOWN = 0x10,
    YSW_MENU_UP = 0x20,
    YSW_MENU_PRESS = 0x40,
    YSW_MENU_PREVIEW = 0x80,
} ysw_menu_cb_activation_t;

#define YSW_MENU_CB_ACTIVATION_MASK 0xf0

typedef enum {
    YSW_MENU_REPEAT = 0x100,
    YSW_MENU_LUCID = 0x200,
    YSW_MENU_END = 0x400,
} ysw_menu_control_t;

#define YSW_MENU_CONTROL_MASK 0xf00

// Common combinations of above flags

#define YSW_MF_NOP (0)
#define YSW_MF_PRESS (YSW_MENU_PRESS)
#define YSW_MF_BUTTON (YSW_MENU_DOWN|YSW_MENU_UP)
#define YSW_MF_COMMAND (YSW_MENU_PRESS|YSW_MENU_RESET)
#define YSW_MF_PREVIEW (YSW_MENU_PREVIEW|YSW_MENU_PRESS)
#define YSW_MF_STICKY (YSW_MENU_PRESS|YSW_MENU_HIDE)
#define YSW_MF_PLUS (YSW_MENU_UP|YSW_MENU_PLUS)
#define YSW_MF_MINUS (YSW_MENU_UP|YSW_MENU_MINUS)
#define YSW_MF_HIDE (YSW_MENU_HIDE)
#define YSW_MF_SHOW (YSW_MENU_SHOW)
#define YSW_MF_TOGGLE (YSW_MENU_TOGGLE)
#define YSW_MF_END (YSW_MENU_END)
#define YSW_MF_LUCID_END (YSW_MENU_LUCID|YSW_MENU_END)

typedef enum {
    YSW_MENU_ENDLINE = -1,
    YSW_MENU_ENDGRID = -2,
} ysw_menu_softkey_t;

typedef struct {
    lv_obj_t *container;
    lv_obj_t *label;
    lv_obj_t *btnmatrix;
    const char **button_map;
} ysw_softkeys_t;

typedef struct ysw_menu_s ysw_menu_t;
typedef struct ysw_menu_item_s ysw_menu_item_t;

typedef void (*ysw_menu_cb_t)(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item);

typedef struct ysw_menu_item_s {
    uint32_t softkey;
    const char *name;
    uint32_t flags;
    ysw_menu_cb_t cb;
    intptr_t value;
    const ysw_menu_item_t *submenu;
} ysw_menu_item_t;

typedef struct ysw_menu_s {
    ysw_bus_t *bus;
    ysw_menu_root_handling_t root_handling; // whether or not to show root menu on menu-
    uint32_t softkey_map_size; // number of scan codes in softmap
    const ysw_menu_softkey_t *softkey_map; // scan codes comprising soft keys
    ysw_array_t *stack; // each element is the base of an array of menu items
    ysw_softkeys_t *softkeys; // null if not currently displaying softkeys
    void *context; // arbitrary context passed through from caller
} ysw_menu_t;

void ysw_menu_on_softkey_down(ysw_menu_t *menu, ysw_event_t *event);

void ysw_menu_on_softkey_up(ysw_menu_t *menu, ysw_event_t *event);

void ysw_menu_on_softkey_pressed(ysw_menu_t *menu, ysw_event_t *event);

void ysw_menu_nop(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item);

void ysw_menu_show(ysw_menu_t *menu);

void ysw_menu_add_glass(ysw_menu_t *menu, lv_obj_t *container);

void ysw_menu_add_sub_menu(ysw_menu_t *menu, const ysw_menu_item_t *sub_menu);

void ysw_menu_remove_sub_menu(ysw_menu_t *menu, const ysw_menu_item_t *sub_menu);

ysw_menu_t *ysw_menu_create(ysw_bus_t *bus, const ysw_menu_item_t *menu_items, const ysw_menu_softkey_t softkey_map[], void *context);

void ysw_menu_free(ysw_menu_t *menu);

void ysw_menu_set_root_handling(ysw_menu_t *menu, ysw_menu_root_handling_t root_handling);
