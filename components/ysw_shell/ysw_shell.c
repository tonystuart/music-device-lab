// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_shell.h"
#include "ysw_app.h"
#include "ysw_bus.h"
#include "ysw_chooser.h"
#include "ysw_common.h"
#include "ysw_editor.h"
#include "ysw_heap.h"
#include "ysw_menu.h"
#include "ysw_popup.h"
#include "ysw_string.h"
#include "ysw_style.h"
#include "ysw_task.h"
#include "zm_music.h"
#include "esp_log.h"

#define TAG "YSW_SHELL"

LV_IMG_DECLARE(ysw_splash_image)

typedef enum {
    S_OPEN_SECTION,
    S_COPY_SECTION,
    S_RENAME_SECTION,
    S_DELETE_SECTION,
} ysw_shell_state_t;

typedef struct {
    ysw_bus_t *bus;
    zm_music_t *music;
    ysw_task_t *task;
    ysw_menu_t *menu;
    QueueHandle_t queue;
    zm_section_t *section; // most recently selected section
    zm_section_x section_index; // next index after delete
    ysw_shell_state_t state;
} ysw_shell_t;

static void start_listening(ysw_shell_t *shell)
{
    ysw_bus_subscribe(shell->bus, YSW_ORIGIN_KEYBOARD, shell->queue);
}

static void start_listening_all(ysw_shell_t *shell)
{
    ysw_bus_subscribe(shell->bus, YSW_ORIGIN_CHOOSER, shell->queue);
    start_listening(shell);
}

static void stop_listening(ysw_shell_t *shell)
{
    ysw_bus_unsubscribe(shell->bus, YSW_ORIGIN_KEYBOARD, shell->queue);
}

static void create_section(ysw_shell_t *shell)
{
    zm_section_t *section = zm_create_section(shell->music);
    ysw_array_push(shell->music->sections, section);
    stop_listening(shell);
    ysw_editor_edit_section(shell->bus, shell->music, section);
    start_listening(shell);
}

static void open_section(ysw_shell_t *shell, zm_section_t *section)
{
    stop_listening(shell);
    ysw_editor_edit_section(shell->bus, shell->music, section);
    start_listening(shell);
}

static void copy_section(ysw_shell_t *shell, zm_section_t *section)
{
    zm_section_t *new_section = zm_copy_section(shell->music, section);
    zm_section_x section_x = ysw_array_find(shell->music->sections, section);
    ysw_array_insert(shell->music->sections, section_x + 1, new_section);
}

static void on_confirm_delete(void *context, ysw_popup_t *popup)
{
    ysw_shell_t *shell = context;
    zm_section_free(shell->music, shell->section);
}

static void delete_section(ysw_shell_t *shell, zm_section_t *section)
{
    ysw_array_t *references = zm_get_section_references(shell->music, section);
    zm_section_x count = ysw_array_get_count(references);
    if (count) {
        ysw_string_t *s = ysw_string_create(128);
        ysw_string_printf(s,
                "You cannot delete\n%s\nbecause it is referenced by the following composition(s):\n",
                section->name);
        for (zm_composition_x i = 0; i < count; i++) {
            zm_composition_t *composition = ysw_array_get(references, i);
            ysw_string_printf(s, "  %s\n", composition->name);
        }
        ysw_string_printf(s, "Please delete them first");
        ysw_popup_config_t config = {
            .type = YSW_MSGBOX_OKAY,
            .context = shell,
            .message = ysw_string_get_chars(s),
            .okay_scan_code = 5,
        };
        ysw_popup_create(&config);
        ysw_string_free(s);
    } else {
        ysw_string_t *s = ysw_string_create(128);
        ysw_string_printf(s, "Delete %s?", section->name);
        ysw_popup_config_t config = {
            .type = YSW_MSGBOX_OKAY_CANCEL,
            .context = shell,
            .message = ysw_string_get_chars(s),
            .on_okay = on_confirm_delete,
            .okay_scan_code = 5,
            .cancel_scan_code = 6,
        };
        ysw_popup_create(&config);
        ysw_string_free(s);
    }
}

static void on_new_section_name(void *context, const char *text)
{
    ysw_shell_t *shell = context;
    shell->section->name = ysw_heap_strdup(text);
    shell->section->tlm = shell->music->settings.clock++;
}

static void rename_section(ysw_shell_t *shell, zm_section_t *section)
{
    ysw_edit_pane_create("Rename Section", section->name, on_new_section_name, shell);
}

static void on_new_section(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_shell_t *shell = menu->context;
    create_section(shell);
}

static void on_open_section(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_shell_t *shell = menu->context;
    shell->state = S_OPEN_SECTION;
    stop_listening(shell);
    ysw_chooser_create(menu->bus, shell->music, shell->section_index, shell);
    start_listening(shell);
}

static void on_copy_section(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_shell_t *shell = menu->context;
    shell->state = S_COPY_SECTION;
    ysw_chooser_create(menu->bus, shell->music, shell->section_index, shell);
}

static void on_rename_section(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_shell_t *shell = menu->context;
    shell->state = S_RENAME_SECTION;
    ysw_chooser_create(menu->bus, shell->music, shell->section_index, shell);
}

static void on_delete_section(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_shell_t *shell = menu->context;
    shell->state = S_DELETE_SECTION;
    ysw_chooser_create(menu->bus, shell->music, shell->section_index, shell);
}

static void on_save(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_shell_t *shell = menu->context;
    zm_save_music(shell->music);
}

// Layout of keycodes generated by keyboard:
//
//    0,  1,      2,  3,  4,      5,  6,  7,  8,
//  9, 10, 11, 12, 13, 14, 15,   16, 17, 18, 19,
//   20, 21,     22, 23, 24,     25, 26, 27, 28,
// 29, 30, 31, 32, 33, 34, 35,   36, 37, 38, 39,

static const ysw_menu_item_t new_menu[] = {
    { YSW_R1_C1, "Music", YSW_MF_COMMAND, on_new_section, 0, NULL },
    { YSW_R1_C2, "Beat", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C3, "Sample", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C4, "Program", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R2_C1, "Chord\nType", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R2_C2, "Arrange-\nment", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R3_C1, "Chord\nStyle", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "New", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t open_menu[] = {
    { YSW_R1_C1, "Music", YSW_MF_COMMAND, on_open_section, 0, NULL },
    { YSW_R1_C2, "Beat", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C3, "Sample", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C4, "Program", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R2_C1, "Chord\nType", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R2_C2, "Arrange-\nment", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R3_C1, "Chord\nStyle", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Open", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t copy_menu[] = {
    { YSW_R1_C1, "Music", YSW_MF_PRESS, on_copy_section, 0, NULL },
    { YSW_R1_C2, "Beat", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C3, "Sample", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C4, "Program", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R2_C1, "Chord\nType", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R2_C2, "Arrange-\nment", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R3_C1, "Chord\nStyle", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Copy", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t rename_menu[] = {
    { YSW_R1_C1, "Music", YSW_MF_PRESS, on_rename_section, 0, NULL },
    { YSW_R1_C2, "Beat", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C3, "Sample", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C4, "Program", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R2_C1, "Chord\nType", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R2_C2, "Arrange-\nment", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R3_C1, "Chord\nStyle", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Rename", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t delete_menu[] = {
    { YSW_R1_C1, "Music", YSW_MF_PRESS, on_delete_section, 0, NULL },
    { YSW_R1_C2, "Beat", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C3, "Sample", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C4, "Program", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R2_C1, "Chord\nType", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R2_C2, "Arrange-\nment", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R3_C1, "Chord\nStyle", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Delete", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t start_menu[] = {
    { YSW_R1_C1, "New", YSW_MF_PLUS, ysw_menu_nop, 0, new_menu },
    { YSW_R1_C2, "Copy", YSW_MF_PLUS, ysw_menu_nop, 0, copy_menu },
    { YSW_R1_C3, "Rename", YSW_MF_PLUS, ysw_menu_nop, 0, rename_menu },
    { YSW_R1_C4, "Delete", YSW_MF_PLUS, ysw_menu_nop, 0, delete_menu },

    { YSW_R2_C1, "Open", YSW_MF_PLUS, ysw_menu_nop, 0, open_menu },

    { YSW_R3_C1, "Save", YSW_MF_COMMAND, on_save, 0, NULL },
    { YSW_R3_C3, "Settings", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C3, "Toggle\nMenu", YSW_MF_TOGGLE, ysw_menu_nop, 0, NULL },

    { 0, "Shell", YSW_MF_END, NULL, 0, NULL },
};

static void on_chooser_select(ysw_shell_t *shell, ysw_event_chooser_select_t *event)
{
    if (event->context == shell) {
        shell->section = event->section;
        shell->section_index = ysw_array_find(shell->music->sections, event->section);
        switch (shell->state) {
            case S_OPEN_SECTION:
                open_section(shell, event->section);
                break;
            case S_COPY_SECTION:
                copy_section(shell, event->section);
                break;
            case S_RENAME_SECTION:
                rename_section(shell, event->section);
                break;
            case S_DELETE_SECTION:
                delete_section(shell, event->section);
                break;
        }
    }
}

static ysw_app_control_t process_event(void *context, ysw_event_t *event)
{
    ysw_shell_t *shell = context;
    switch (event->header.type) {
        case YSW_EVENT_KEY_DOWN:
            ysw_menu_on_key_down(shell->menu, event);
            break;
        case YSW_EVENT_KEY_UP:
            ysw_menu_on_key_up(shell->menu, event);
            break;
        case YSW_EVENT_KEY_PRESSED:
            ysw_menu_on_key_pressed(shell->menu, event);
            break;
        case YSW_EVENT_CHOOSER_SELECT:
            on_chooser_select(shell, &event->chooser_select);
            break;
        default:
            break;
    }
    return YSW_APP_CONTINUE;
}

static void create_splash_screen(ysw_shell_t *shell)
{
    ysw_splash_t *splash = ysw_heap_allocate(sizeof(ysw_splash_t));
    splash->container = C(lv_obj_create(lv_scr_act(), NULL));
    lv_obj_set_size(splash->container, 320, 240);
    lv_obj_align(splash->container, NULL, LV_ALIGN_CENTER, 0, 0);

    splash->label = C(lv_label_create(splash->container, NULL));
    lv_label_set_align(splash->label, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text(splash->label,
            "Welcome to Music Machine\n"
            "Version 4.0.0\n"
            "\n"
            "Tap Screen or Press Menu (#) to Begin");
    lv_obj_align(splash->label, NULL, LV_ALIGN_IN_TOP_MID, 0, 30); // after text is set

    splash->image = C(lv_img_create(splash->container, NULL));
    lv_img_set_src(splash->image, &ysw_splash_image);
    lv_obj_align(splash->image, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0); // after image is set
    ysw_style_splash(splash->container, splash->label, splash->image);

    ysw_menu_add_glass(shell->menu, splash->container);
}

void ysw_shell_create(ysw_bus_t *bus, zm_music_t *music)
{
    ysw_shell_t *shell = ysw_heap_allocate(sizeof(ysw_shell_t));
    shell->bus = bus;
    shell->music = music;
    shell->menu = ysw_menu_create(bus, start_menu, ysw_app_softkey_map, shell);
    shell->queue = ysw_app_create_queue();

    create_splash_screen(shell);
    start_listening_all(shell);
    ysw_app_handle_events(shell->queue, process_event, shell);
}

