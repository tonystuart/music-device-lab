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

static inline const ysw_menu_item_t *get_items(ysw_menu_t *menu)
{
    return ysw_array_get_top(menu->stack);
}

// TODO: consider using binary search

static const ysw_menu_item_t *find_item_by_scan_code(ysw_menu_t *menu, uint32_t scan_code)
{
    uint32_t stack_size = ysw_array_get_count(menu->stack);
    for (int32_t i = stack_size - 1; i >= 0; i--) {
        const ysw_menu_item_t *menu_item = ysw_array_get(menu->stack, i);
        while (menu_item->name) {
            if (menu_item->scan_code == scan_code) {
                return menu_item;
            }
            menu_item++;
        }
    }
    return NULL;
}

static int32_t find_scan_code_by_button_index(ysw_menu_t *menu, int32_t button_index)
{
    uint16_t label_index = 0;
    const ysw_menu_softmap_t *softmap = menu->softmap;
    while (*softmap != YSW_MENU_ENDGRID) {
        if (*softmap != YSW_MENU_ENDLINE) {
            if (button_index == label_index++) {
                return *softmap;
            }
        }
        softmap++;
    }
    return -1;
}

static void event_handler(lv_obj_t *btnmatrix, lv_event_t button_event)
{
    ysw_menu_t *menu = lv_obj_get_user_data(btnmatrix);
    if (menu) {
        uint16_t button_index = lv_btnmatrix_get_active_btn(btnmatrix);
        if (button_index != LV_BTNMATRIX_BTN_NONE) {
            int32_t item_index = find_scan_code_by_button_index(menu, button_index);
            if (item_index != -1) {
                if (button_event == LV_EVENT_PRESSED) {
                    ysw_event_t event = {
                        .header.origin = YSW_ORIGIN_KEYBOARD,
                        .header.type = YSW_EVENT_KEY_DOWN,
                        .key_down.key = item_index,
                    };
                    ysw_menu_on_key_down(menu, &event);
                } else if (button_event == LV_EVENT_RELEASED) {
                    ysw_event_t event = {
                        .header.origin = YSW_ORIGIN_KEYBOARD,
                        .header.type = YSW_EVENT_KEY_UP,
                        .key_up.key = item_index,
                    };
                    ysw_menu_on_key_up(menu, &event);
                } else if (button_event == LV_EVENT_VALUE_CHANGED) {
                    ysw_event_t event = {
                        .header.origin = YSW_ORIGIN_KEYBOARD,
                        .header.type = YSW_EVENT_KEY_PRESSED,
                        .key_pressed.key = item_index,
                    };
                    ysw_menu_on_key_pressed(menu, &event);
                }
            }
        }
    }
}

static uint32_t get_button_map_size(ysw_menu_t *menu)
{
    const ysw_menu_softmap_t *softmap = menu->softmap;
    while (*softmap != YSW_MENU_ENDGRID) {
        softmap++;
    }
    return (softmap - menu->softmap) + 1; // +1 for YSW_MENU_ENDGRID item
}

static const char** create_button_map(ysw_menu_t *menu)
{
    const char **map = ysw_heap_allocate(menu->softmap_size * sizeof(char *));
    const char **p = map;

    const ysw_menu_softmap_t *softmap = menu->softmap;
    while (*softmap != YSW_MENU_ENDGRID) {
        if (*softmap == YSW_MENU_ENDLINE) {
            *p = "\n";
        } else {
            const ysw_menu_item_t *menu_item = find_item_by_scan_code(menu, *softmap);
            *p = menu_item->name;
        }
        softmap++;
        p++;
    }
    *p = "";

    return map;
}

static void show_softkeys(ysw_menu_t *menu)
{
    lv_obj_t *container = lv_obj_create(lv_scr_act(), NULL);
    assert(container);

    lv_obj_set_style_local_bg_color(container, 0, 0, LV_COLOR_BLACK);
    lv_obj_set_style_local_bg_opa(container, 0, 0, LV_OPA_100);

    lv_obj_set_style_local_text_color(container, 0, 0, LV_COLOR_CYAN);
    lv_obj_set_style_local_text_opa(container, 0, 0, LV_OPA_100);

    lv_obj_set_size(container, 320, 240);
    lv_obj_align(container, NULL, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *btnmatrix = lv_btnmatrix_create(container, NULL);
    lv_btnmatrix_set_align(btnmatrix, LV_LABEL_ALIGN_CENTER);

    lv_obj_set_style_local_bg_color(btnmatrix, LV_BTNMATRIX_PART_BTN, 0, LV_COLOR_CYAN);
    lv_obj_set_style_local_bg_opa(btnmatrix, LV_BTNMATRIX_PART_BTN, 0, LV_OPA_30);

    lv_obj_set_style_local_bg_color(btnmatrix, LV_BTNMATRIX_PART_BTN, LV_STATE_PRESSED, LV_COLOR_BLACK);
    lv_obj_set_style_local_bg_opa(btnmatrix, LV_BTNMATRIX_PART_BTN, LV_STATE_PRESSED, LV_OPA_50);

    lv_obj_set_style_local_pad_all(btnmatrix, LV_BTNMATRIX_PART_BG, 0, 5);
    lv_obj_set_style_local_pad_inner(btnmatrix, LV_BTNMATRIX_PART_BG, 0, 5);

    lv_obj_set_size(btnmatrix, 320, 240);
    lv_obj_align(btnmatrix, NULL, LV_ALIGN_CENTER, 0, 0);

    const char **button_map = create_button_map(menu);
    lv_btnmatrix_set_map(btnmatrix, button_map);
    lv_obj_set_user_data(btnmatrix, menu);
    lv_obj_set_event_cb(btnmatrix, event_handler);

    menu->softkeys = ysw_heap_allocate(sizeof(ysw_softkeys_t));
    menu->softkeys->container = container;
    menu->softkeys->button_map = button_map;
}

static void hide_softkeys(ysw_menu_t *menu)
{
    assert(menu->softkeys);
    lv_obj_del(menu->softkeys->container);
    ysw_heap_free(menu->softkeys->button_map);
    ysw_heap_free(menu->softkeys);
    menu->softkeys = NULL;
}

static void pop_all(ysw_menu_t *menu)
{
    if (menu->softkeys) {
        hide_softkeys(menu);
        while (ysw_array_get_count(menu->stack) > 1) {
            ysw_array_pop(menu->stack);
        }
    }
}

static void pop_menu(ysw_menu_t *menu)
{
    if (menu->softkeys) {
        hide_softkeys(menu);
        if (ysw_array_get_count(menu->stack) > 1) {
            ysw_array_pop(menu->stack);
            show_softkeys(menu);
            lv_indev_wait_release(lv_indev_get_act()); // suppress events while button is still down
        }
    } else {
        show_softkeys(menu);
    }
}

static void open_menu(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    if (menu->softkeys) {
        if (value) {
            hide_softkeys(menu);
            ysw_array_push(menu->stack, value);
            show_softkeys(menu);
        }
    } else {
        while (ysw_array_get_count(menu->stack) > 1) {
            ysw_array_pop(menu->stack);
        }
        show_softkeys(menu);
    }
    menu->wait_release = true;
}

static void close_menu(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
    if (menu->softkeys) {
        hide_softkeys(menu);
    }
}

void ysw_menu_on_key_down(ysw_menu_t *menu, ysw_event_t *event)
{
    const ysw_menu_item_t *menu_item = find_item_by_scan_code(menu, event->key_down.key);
    if (menu_item) {
        if (menu_item->flags & YSW_MENU_CLOSE) {
            close_menu(menu, event, menu_item->value);
        }
        if (menu_item->flags & YSW_MENU_DOWN) {
            menu_item->cb(menu, event, menu_item->value);
        }
        if (menu_item->flags & YSW_MENU_OPEN) {
            open_menu(menu, event, menu_item->value);
        }
    }
}

void ysw_menu_on_key_up(ysw_menu_t *menu, ysw_event_t *event)
{
    menu->wait_release = false;
    const ysw_menu_item_t *menu_item = find_item_by_scan_code(menu, event->key_up.key);
    if (menu_item->flags & YSW_MENU_UP) {
        menu_item->cb(menu, event, menu_item->value);
    }
    if (menu_item->flags & YSW_MENU_POP_ALL) {
        pop_all(menu);
    } else if (menu_item->flags & YSW_MENU_POP) {
        pop_menu(menu);
    }
}

void ysw_menu_on_key_pressed(ysw_menu_t *menu, ysw_event_t *event)
{
    if (!menu->wait_release) {
        const ysw_menu_item_t *menu_item = find_item_by_scan_code(menu, event->key_pressed.key);
        if (menu_item->flags & YSW_MENU_PRESS) {
            menu_item->cb(menu, event, menu_item->value);
        }
    }
}

ysw_menu_t *ysw_menu_create(const ysw_menu_item_t *menu_items, const ysw_menu_softmap_t softmap[], void *caller_context)
{
    ysw_menu_t *menu = ysw_heap_allocate(sizeof(ysw_menu_t));
    menu->softmap = softmap;
    menu->caller_context = caller_context;
    menu->stack = ysw_array_create(4);
    menu->softmap_size = get_button_map_size(menu);
    ysw_array_push(menu->stack, (void*)menu_items);
    return menu;
}

void ysw_menu_nop(ysw_menu_t *menu, ysw_event_t *event, void *value)
{
}

