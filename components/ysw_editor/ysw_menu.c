// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_menu.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_heap.h"
#include "lvgl.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_MENU"

static uint32_t get_map_item_count(ysw_menu_t *menu)
{
    uint32_t map_item_count = 0;
    const ysw_menu_item_t *menu_item = menu->menu_items;
    while (menu_item->name) {
        if (menu_item->flags & 0x80) {
            map_item_count++;
        }
        if (menu_item->flags & 0x40) {
            map_item_count++;
        }
        menu_item++;
    }
    map_item_count++; // empty string - end of map indicator
    return map_item_count;
}

static const char** get_map_items(ysw_menu_t *menu)
{
    uint32_t map_item_count = get_map_item_count(menu);
    const char **map = ysw_heap_allocate(map_item_count * sizeof(char *));
    const char **p = map;

    const ysw_menu_item_t *menu_item = menu->menu_items;
    while (menu_item->name) {
        if (menu_item->flags & 0x80) {
            *p++ = menu_item->name;
        }
        if (menu_item->flags & 0x40) {
            map_item_count++;
            *p++ = "\n";
        }
        menu_item++;
    }

    *p = "";
    return map;
}

static void open_menu(ysw_menu_t *menu)
{
    menu->container = lv_obj_create(lv_scr_act(), NULL);
    assert(menu->container);

    lv_obj_set_style_local_bg_color(menu->container, 0, 0, LV_COLOR_BLACK);
    lv_obj_set_style_local_bg_opa(menu->container, 0, 0, LV_OPA_50);

    lv_obj_set_style_local_text_color(menu->container, 0, 0, LV_COLOR_CYAN);
    lv_obj_set_style_local_text_opa(menu->container, 0, 0, LV_OPA_100);

    lv_obj_set_size(menu->container, 320, 240);
    lv_obj_align(menu->container, NULL, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *btnmatrix = lv_btnmatrix_create(menu->container, NULL);

    lv_obj_set_style_local_border_width(btnmatrix, LV_BTNMATRIX_PART_BTN, 0, 1);
    lv_obj_set_style_local_border_color(btnmatrix, LV_BTNMATRIX_PART_BTN, 0, LV_COLOR_CYAN);
    lv_obj_set_style_local_border_opa(btnmatrix, LV_BTNMATRIX_PART_BTN, 0, LV_OPA_100);

    lv_obj_set_style_local_pad_all(btnmatrix, LV_BTNMATRIX_PART_BG, 0, 5);
    lv_obj_set_style_local_pad_inner(btnmatrix, LV_BTNMATRIX_PART_BG, 0, 5);

    lv_obj_set_size(btnmatrix, 320, 240);
    lv_obj_align(btnmatrix, NULL, LV_ALIGN_CENTER, 0, 0);

    const char **map_items = get_map_items(menu);
    lv_obj_set_user_data(menu->container, map_items);
    lv_btnmatrix_set_map(btnmatrix, map_items);
}

static void close_menu(ysw_menu_t *menu)
{
    const char **map_items = lv_obj_get_user_data(menu->container);
    assert(map_items);
    ysw_heap_free(map_items);
    lv_obj_del(menu->container);
    menu->container = NULL;
}

void ysw_menu_on_key_down(ysw_menu_t *menu, ysw_event_t *event, void *caller_context)
{
    const ysw_menu_item_t *menu_item = menu->menu_items + event->key_down.key;
    if (menu_item->flags & YSW_MENU_DOWN) {
        menu_item->cb(menu, event, caller_context, menu_item->value);
    }
}

void ysw_menu_on_key_up(ysw_menu_t *menu, ysw_event_t *event, void *caller_context)
{
    const ysw_menu_item_t *menu_item = menu->menu_items + event->key_down.key;
    if (menu_item->flags & YSW_MENU_UP) {
        menu_item->cb(menu, event, caller_context, menu_item->value);
    }
}

void ysw_menu_on_key_pressed(ysw_menu_t *menu, ysw_event_t *event, void *caller_context)
{
    const ysw_menu_item_t *menu_item = menu->menu_items + event->key_down.key;
    if (menu_item->flags & YSW_MENU_PRESS) {
        menu_item->cb(menu, event, caller_context, menu_item->value);
    }
}

ysw_menu_t *ysw_menu_create(const ysw_menu_item_t *menu_items)
{
    ysw_menu_t *menu = ysw_heap_allocate(sizeof(ysw_menu_t));
    menu->menu_items = menu_items;
    menu->menu_stack = ysw_array_create(4);
    return menu;
}

void ysw_menu_nop(ysw_menu_t *menu, ysw_event_t *event, void *caller_context, void *value)
{
}

void ysw_menu_on_open(ysw_menu_t *menu, ysw_event_t *event, void *caller_context, void *value)
{
    if (event->header.type == YSW_EVENT_KEY_DOWN) {
        if (menu->container) {
            if (value) {
                close_menu(menu);
                ysw_array_push(menu->menu_stack, (void*)menu->menu_items);
                menu->menu_items = value;
                open_menu(menu);
            }
        } else {
            open_menu(menu);
        }
    }
}

void ysw_menu_on_close(ysw_menu_t *menu, ysw_event_t *event, void *caller_context, void *value)
{
    if (event->header.type == YSW_EVENT_KEY_DOWN) {
        if (menu->container) {
            close_menu(menu);
            if (ysw_array_get_count(menu->menu_stack)) {
                menu->menu_items = ysw_array_pop(menu->menu_stack);
                open_menu(menu);
            }
        }
    }
}
