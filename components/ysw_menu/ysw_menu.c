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
#include "ysw_string.h"
#include "ysw_style.h"
#include "lvgl.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "YSW_MENU"

static void show_menu(ysw_menu_t *menu);
static void hide_menu(ysw_menu_t *menu);

static inline ysw_menu_item_t *get_items(ysw_menu_t *menu)
{
    return ysw_array_get_top(menu->stack);
}

static ysw_menu_item_t *find_item_by_softkey(ysw_menu_t *menu, uint32_t softkey)
{
    ysw_menu_item_t *menu_item = ysw_array_get_top(menu->stack);
    while (!(menu_item->flags & YSW_MENU_END)) {
        if (menu_item->softkey == softkey) {
            return menu_item;
        }
        menu_item++;
    }
    return NULL;
}

static int32_t find_softkey_by_button_index(ysw_menu_t *menu, int32_t button_index)
{
    uint16_t label_index = 0;
    const ysw_menu_softkey_t *softkey = menu->softkey_map;
    while (*softkey != YSW_MENU_ENDGRID) {
        if (*softkey != YSW_MENU_ENDLINE) {
            if (button_index == label_index++) {
                return *softkey;
            }
        }
        softkey++;
    }
    return -1;
}

static void fire_events(ysw_menu_t *menu, uint32_t softkey)
{
    ysw_event_softkey_down_t key_down = {
        .softkey = softkey,
    };
    ysw_event_fire_softkey_down(menu->bus, &key_down);

    ysw_event_softkey_pressed_t key_pressed = {
        .softkey = softkey,
    };
    ysw_event_fire_softkey_pressed(menu->bus, &key_pressed);

    ysw_event_softkey_up_t key_up = {
        .softkey = softkey,
    };
    ysw_event_fire_softkey_up(menu->bus, &key_up);
}

static void event_handler(lv_obj_t *btnmatrix, lv_event_t button_event)
{
    ysw_menu_t *menu = lv_obj_get_user_data(btnmatrix);
    if (menu) {
        uint16_t button_index = lv_btnmatrix_get_active_btn(btnmatrix);
        if (button_index != LV_BTNMATRIX_BTN_NONE) {
            int32_t softkey = find_softkey_by_button_index(menu, button_index);
            if (softkey != -1) {
                ysw_menu_item_t *item = find_item_by_softkey(menu, softkey);
                if (item) { // may have a softkey but no menu_item
                    if (item->flags & YSW_MENU_PREVIEW) {
                        if (button_event == LV_EVENT_PRESSED) {
                            // note that lv_obj_set_hidden cancels btnmatrix event processing
                            ysw_style_softkeys_hidden(menu->softkeys->container,
                                    menu->softkeys->label,
                                    menu->softkeys->btnmatrix);
                            fire_events(menu, softkey);
                        } else if (button_event == LV_EVENT_LONG_PRESSED_REPEAT &&
                                item->flags & YSW_MENU_REPEAT) {
                            // called every LV_INDEV_LONG_PRESS_REP_TIME ms after LV_INDEV_LONG_PRESS_TIME
                            fire_events(menu, softkey);
                        } else if (button_event == LV_EVENT_RELEASED) {
                            ysw_style_softkeys(menu->softkeys->container,
                                    menu->softkeys->label,
                                    menu->softkeys->btnmatrix);
                        }
                    } else if (button_event == LV_EVENT_RELEASED) {
                        fire_events(menu, softkey);
                    }
                }
            }
        }
    }
}

static uint32_t get_menu_flags(ysw_menu_t *menu)
{
    uint32_t flags = 0;
    ysw_menu_item_t *menu_item = ysw_array_get_top(menu->stack);
    if (menu_item) {
        while (!(menu_item->flags & YSW_MENU_END)) {
            // find last item
            menu_item++;
        }
        flags = menu_item->flags;
    }
    return flags;
}

static void get_menu_name(ysw_menu_t *menu, ysw_string_t *s)
{
    uint32_t stack_size = ysw_array_get_count(menu->stack);
    for (uint32_t i = 0; i < stack_size; i++) {
        ysw_menu_item_t *menu_item = ysw_array_get(menu->stack, i);
        while (!(menu_item->flags & YSW_MENU_END)) {
            // find last item
            menu_item++;
        }
        if (ysw_string_get_length(s)) {
            ysw_string_append_chars(s, " > ");
        }
        ysw_string_append_chars(s, menu_item->name);
    }
}

static uint32_t get_button_map_size(ysw_menu_t *menu)
{
    const ysw_menu_softkey_t *softkey = menu->softkey_map;
    while (*softkey != YSW_MENU_ENDGRID) {
        softkey++;
    }
    return (softkey - menu->softkey_map) + 1; // +1 for YSW_MENU_ENDGRID item
}

static const char** create_button_map(ysw_menu_t *menu)
{
    const char **map = ysw_heap_allocate(menu->softkey_map_size * sizeof(char *));
    const char **p = map;

    const ysw_menu_softkey_t *softkey = menu->softkey_map;
    while (*softkey != YSW_MENU_ENDGRID) {
        if (*softkey == YSW_MENU_ENDLINE) {
            *p = "\n";
        } else {
            ysw_menu_item_t *menu_item = find_item_by_softkey(menu, *softkey);
            if (menu_item) {
                *p = menu_item->name;
            } else {
                *p = " ";
            }
        }
        softkey++;
        p++;
    }
    *p = "";

    return map;
}

// TODO: factor out ysw_softkeys.c

static void show_softkeys(ysw_menu_t *menu)
{
    lv_obj_t *container = lv_obj_create(lv_scr_act(), NULL);
    assert(container);

    lv_obj_set_size(container, 320, 240);
    lv_obj_align(container, NULL, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *label = lv_label_create(container, NULL);

    lv_label_set_long_mode(label, LV_LABEL_LONG_SROLL_CIRC);
    lv_obj_set_width(label, 320);
    lv_obj_align(label, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 2);

    lv_label_set_align(label, LV_LABEL_ALIGN_CENTER);

    ysw_string_t *name = ysw_string_create(128);
    get_menu_name(menu, name);
    lv_label_set_text(label, ysw_string_get_chars(name));
    ysw_string_free(name);

    lv_obj_t *btnmatrix = lv_btnmatrix_create(container, NULL);
    lv_btnmatrix_set_align(btnmatrix, LV_LABEL_ALIGN_CENTER);

    uint32_t flags = get_menu_flags(menu);
    if (flags & YSW_MENU_LUCID) {
        ysw_style_softkeys_lucid(container, label, btnmatrix);
    } else {
        ysw_style_softkeys(container, label, btnmatrix);
    }

    lv_obj_set_size(btnmatrix, 320, 230);
    lv_obj_align(btnmatrix, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);

    const char **button_map = create_button_map(menu);
    lv_btnmatrix_set_map(btnmatrix, button_map);
    lv_obj_set_user_data(btnmatrix, menu);
    lv_obj_set_event_cb(btnmatrix, event_handler);

    menu->softkeys = ysw_heap_allocate(sizeof(ysw_softkeys_t));
    menu->softkeys->container = container;
    menu->softkeys->label = label;
    menu->softkeys->btnmatrix = btnmatrix;
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

static void plus_menu(ysw_menu_t *menu, ysw_menu_item_t *item)
{
    if (menu->softkeys) {
        if (item->submenu) {
            hide_softkeys(menu);
            ysw_array_push(menu->stack, (void*)item->submenu);
            show_softkeys(menu);
        }
    } else {
        show_softkeys(menu);
    }
}

static void minus_menu(ysw_menu_t *menu)
{
    if (menu->softkeys) {
        hide_softkeys(menu);
        if (ysw_array_get_count(menu->stack) > 1) {
            ysw_array_pop(menu->stack);
            if (ysw_array_get_count(menu->stack) > 1 || menu->root_handling == YSW_MENU_ROOT_SHOW) {
                show_softkeys(menu);
            }
        }
    } else {
        show_softkeys(menu);
    }
}

static void show_menu(ysw_menu_t *menu)
{
    if (!menu->softkeys) {
        show_softkeys(menu);
    }
}

static void hide_menu(ysw_menu_t *menu)
{
    if (menu->softkeys) {
        hide_softkeys(menu);
    }
}

static void toggle_menu(ysw_menu_t *menu)
{
    if (menu->softkeys) {
        hide_softkeys(menu);
    } else {
        show_softkeys(menu);
    }
}

static void reset_menu(ysw_menu_t *menu)
{
    if (menu->softkeys) {
        hide_softkeys(menu);
    }
    while (ysw_array_get_count(menu->stack) > 1) {
        ysw_array_pop(menu->stack);
    }
}

void ysw_menu_on_softkey_down(ysw_menu_t *menu, ysw_event_t *event)
{
    ysw_menu_item_t *menu_item = find_item_by_softkey(menu, event->softkey_down.softkey);
    if (menu_item && menu_item->flags & YSW_MENU_DOWN) {
        menu_item->cb(menu, event, menu_item);
    }
}

void ysw_menu_on_softkey_up(ysw_menu_t *menu, ysw_event_t *event)
{
    ysw_menu_item_t *menu_item = find_item_by_softkey(menu, event->softkey_up.softkey);
    if (menu_item) {
        if (menu_item->flags & YSW_MENU_UP) {
            menu_item->cb(menu, event, menu_item);
        }
        ysw_menu_action_t action = menu_item->flags & YSW_MENU_ACTION_MASK;
        switch (action) {
            case YSW_MENU_PLUS:
                plus_menu(menu, menu_item);
                break;
            case YSW_MENU_MINUS:
                minus_menu(menu);
                break;
            case YSW_MENU_SHOW:
                show_menu(menu);
                break;
            case YSW_MENU_HIDE:
                hide_menu(menu);
                break;
            case YSW_MENU_TOGGLE:
                toggle_menu(menu);
                break;
            case YSW_MENU_RESET:
                reset_menu(menu);
                break;
        }
    }
}

void ysw_menu_on_softkey_pressed(ysw_menu_t *menu, ysw_event_t *event)
{
    ysw_menu_item_t *menu_item = find_item_by_softkey(menu, event->softkey_pressed.softkey);
    if (menu_item && menu_item->flags & YSW_MENU_PRESS) {
        menu_item->cb(menu, event, menu_item);
    }
}

void ysw_menu_nop(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
}

ysw_menu_t *ysw_menu_create(ysw_bus_t *bus, const ysw_menu_item_t *menu_items, const ysw_menu_softkey_t softkey_map[], void *context)
{
    ysw_menu_t *menu = ysw_heap_allocate(sizeof(ysw_menu_t));
    menu->bus = bus;
    menu->softkey_map = softkey_map;
    menu->context = context;
    menu->stack = ysw_array_create(4);
    menu->softkey_map_size = get_button_map_size(menu);
    ysw_array_push(menu->stack, (ysw_menu_item_t*)menu_items);
    return menu;
}

void ysw_menu_free(ysw_menu_t *menu)
{
    if (menu->softkeys) {
        hide_softkeys(menu);
    }
    ysw_array_free(menu->stack);
    ysw_heap_free(menu);
}

void ysw_menu_show(ysw_menu_t *menu)
{
    show_menu(menu);
}

static void on_glass_event(struct _lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        ysw_menu_t *menu = lv_obj_get_user_data(obj);
        ysw_menu_show(menu);
    }
}

void ysw_menu_add_glass(ysw_menu_t *menu, lv_obj_t *container)
{
    // We don't use lv_layer_top to detect click because it would block menu clicks
    lv_obj_t *glass = lv_obj_create(container, NULL);
    lv_obj_set_size(glass, 320, 240);
    lv_obj_align(glass, NULL, LV_ALIGN_CENTER, 0, 0);

    lv_obj_set_user_data(glass, menu);
    lv_obj_set_event_cb(glass, on_glass_event);
    lv_obj_set_click(glass, true);
}

void ysw_menu_add_sub_menu(ysw_menu_t *menu, const ysw_menu_item_t *sub_menu)
{
    ysw_array_push(menu->stack, (ysw_menu_item_t*)sub_menu);
}

void ysw_menu_remove_sub_menu(ysw_menu_t *menu, const ysw_menu_item_t *sub_menu)
{
    ysw_array_pop(menu->stack);
}

void ysw_menu_set_root_handling(ysw_menu_t *menu, ysw_menu_root_handling_t root_handling)
{
    menu->root_handling = root_handling;
}

