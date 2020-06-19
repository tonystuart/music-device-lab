// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_auto_play.h"
#include "ysw_cs.h"
#include "ysw_csc.h"
#include "ysw_cse.h"
#include "ysw_csp.h"
#include "ysw_heap.h"
#include "ysw_main_seq.h"
#include "ysw_mb.h"
#include "ysw_mfw.h"
#include "ysw_music.h"
#include "ysw_name.h"
#include "ysw_sdb.h"
#include "ysw_sn.h"
#include "ysw_ui.h"

#include "lvgl.h"
#include "esp_log.h"

#define TAG "YSW_CSL"

typedef struct ysw_csl_s ysw_csl_t;
typedef void (*csl_close_cb_t)(void *context, ysw_csl_t *csl);

typedef struct {
    ysw_music_t *music;
    uint32_t cs_index;
    ysw_cs_t *clipboard_cs;
    csl_close_cb_t close_cb;
    void *close_cb_context;
} ysw_csl_controller_t;

typedef struct ysw_csl_s {
    ysw_ui_panel_t panel;
    ysw_csl_controller_t controller;
} ysw_csl_t;

static void clear_selection_highlight(ysw_csl_t *csl)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->controller.music);
    if (csl->controller.cs_index < cs_count) {
        lv_obj_t *scrl = lv_page_get_scrl(csl->panel.body.page);
        lv_obj_t *child = ysw_ui_child_at_index(scrl, csl->controller.cs_index);
        if (child) {
            //lv_obj_set_style_local_border_width(child, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
            lv_obj_set_style_local_text_color(child, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
        }
    }
    lv_label_set_text(csl->panel.footer.info.label, "");
}

static void display_selection_highlight(ysw_csl_t *csl)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->controller.music);
    if (csl->controller.cs_index < cs_count) {
        lv_obj_t *scrl = lv_page_get_scrl(csl->panel.body.page);
        lv_obj_t *child = ysw_ui_child_at_index(scrl, csl->controller.cs_index);
        if (child) {
            //lv_obj_set_style_local_border_width(child, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
            lv_obj_set_style_local_text_color(child, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_YELLOW);
            ysw_ui_ensure_visible(child, true);
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "%d of %d", csl->controller.cs_index + 1, cs_count);
        lv_label_set_text(csl->panel.footer.info.label, buf);
    }
}

static void on_row_event(lv_obj_t *obj, lv_event_t event);

static void display_chord_styles(ysw_csl_t *csl)
{
    lv_label_set_text_static(csl->panel.footer.info.label, "");
    uint32_t cs_count = ysw_music_get_cs_count(csl->controller.music);
    lv_page_clean(csl->panel.body.page);
    if (cs_count) {
        lv_page_set_scrl_layout(csl->panel.body.page, LV_LAYOUT_OFF);
        for (uint32_t i = 0; i < cs_count; i++) {
            // TODO: consider factoring out function(s)
            ysw_cs_t *cs = ysw_music_get_cs(csl->controller.music, i);
            lv_obj_t *obj = lv_obj_create(csl->panel.body.page, NULL);
            lv_obj_set_size(obj, 300, 34);
            ysw_ui_adjust_styles(obj);
            lv_obj_set_user_data(obj, csl);
            lv_obj_t *label = lv_label_create(obj, NULL);
            lv_obj_align(label, obj, LV_ALIGN_IN_LEFT_MID, 5, 0);
            lv_label_set_text_static(label, cs->name);
            lv_obj_t *csp = ysw_csp_create(obj);
            lv_obj_align(csp, obj, LV_ALIGN_IN_RIGHT_MID, -2, 0);
            ysw_csp_set_cs(csp, cs);
            lv_obj_set_event_cb(obj, on_row_event);
        }
        lv_page_set_scrl_layout(csl->panel.body.page, LV_LAYOUT_COLUMN_MID);
        if (csl->controller.cs_index >= cs_count) {
            csl->controller.cs_index = cs_count - 1;
        }
        display_selection_highlight(csl);
    } else {
        csl->controller.cs_index = 0;
        clear_selection_highlight(csl);
    }
}

static void on_csc_close(ysw_csl_t *csl, ysw_csc_t *csc);

static void edit_cs(ysw_csl_t *csl, uint32_t cs_index)
{
    ysw_csc_t *csc = ysw_csc_create(csl->controller.music, cs_index);
    ysw_csc_set_close_cb(csc, on_csc_close, csl);
}

static void create_cs(ysw_csl_t *csl, uint32_t cs_index)
{
    ysw_csc_t *csc = ysw_csc_create_new(csl->controller.music, cs_index);
    ysw_csc_set_close_cb(csc, on_csc_close, csl);
}

static void calculate_insert_index(ysw_csl_t *csl)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->controller.music);
    if (csl->controller.cs_index < cs_count) {
        csl->controller.cs_index++; // insert afterwards
    } else {
        csl->controller.cs_index = cs_count; // insert at end (or beginning if empty)
    }
}

static void on_short_click(lv_obj_t *obj)
{
    ysw_csl_t *csl = lv_obj_get_user_data(obj);
    if (csl) {
        uint32_t cs_index = ysw_ui_get_index_of_child(obj);
        if (cs_index != csl->controller.cs_index) {
            clear_selection_highlight(csl);
            csl->controller.cs_index = cs_index;
            display_selection_highlight(csl);
        }
    }
}

static void on_long_press(lv_obj_t *obj)
{
    ysw_csl_t *csl = lv_obj_get_user_data(obj);
    if (csl) {
        uint32_t cs_index = ysw_ui_get_index_of_child(obj);
        if (cs_index != -1) {
            edit_cs(csl, cs_index);
        }
    }
}

static void on_row_event(lv_obj_t *obj, lv_event_t event)
{
    char type[128];
    ysw_ui_get_obj_type(obj, type, sizeof(type));
    switch (event) {
        case LV_EVENT_PRESSED:
            printf("Pressed, obj=%p, type=%s\n", obj, type);
            break;

        case LV_EVENT_SHORT_CLICKED:
            printf("Short clicked, obj=%p, type=%s\n", obj, type);
            on_short_click(obj);
            break;

        case LV_EVENT_CLICKED:
            printf("Clicked\n");
            break;

        case LV_EVENT_LONG_PRESSED:
            printf("Long press\n");
            on_long_press(obj);
            break;

        case LV_EVENT_LONG_PRESSED_REPEAT:
            printf("Long press repeat\n");
            break;

        case LV_EVENT_RELEASED:
            printf("Released\n");
            break;
    }

    /*Etc.*/
}

static void on_csc_close(ysw_csl_t *csl, ysw_csc_t *csc)
{
    csl->controller.cs_index = csc->cs_index;
    display_chord_styles(csl);
}

static void on_auto_play_all(ysw_csl_t *csl, ysw_auto_play_t auto_play)
{
    ysw_cse_gs.auto_play_all = auto_play;
}

static void on_auto_play_last(ysw_csl_t *csl, ysw_auto_play_t auto_play)
{
    ysw_cse_gs.auto_play_last = auto_play;
}

static void on_multiple_selection(ysw_csl_t *csl, bool multiple_selection)
{
    ysw_cse_gs.multiple_selection = multiple_selection;
}

static void on_settings(ysw_csl_t *csl, lv_obj_t *btn)
{
    ysw_sdb_t *sdb = ysw_sdb_create("Chord Style Editor Settings", csl);
    ysw_sdb_add_choice(sdb, "Multiple Selection",
            ysw_cse_gs.multiple_selection, "No\nYes", on_multiple_selection);
    ysw_sdb_add_separator(sdb, "Auto Play Settings");
    ysw_sdb_add_choice(sdb, "On Change",
            ysw_cse_gs.auto_play_all, ysw_auto_play_options, on_auto_play_all);
    ysw_sdb_add_choice(sdb, "On Click",
            ysw_cse_gs.auto_play_last, ysw_auto_play_options, on_auto_play_last);
}

static void on_save(ysw_csl_t *csl, lv_obj_t *btn)
{
    ysw_mfw_write(csl->controller.music);
}

static void on_new(ysw_csl_t *csl, lv_obj_t *btn)
{
    create_cs(csl, csl->controller.cs_index);
}

static void on_copy(ysw_csl_t *csl, lv_obj_t *btn)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->controller.music);
    if (csl->controller.cs_index < cs_count) {
        if (csl->controller.clipboard_cs) {
            ysw_cs_free(csl->controller.clipboard_cs);
        }
        ysw_cs_t *cs = ysw_music_get_cs(csl->controller.music, csl->controller.cs_index);
        csl->controller.clipboard_cs = ysw_cs_copy(cs);
    } else {
        ysw_mb_nothing_selected();
    }
}

static void on_paste(ysw_csl_t *csl, lv_obj_t *btn)
{
    if (csl->controller.clipboard_cs) {
        ysw_cs_t *cs = ysw_cs_copy(csl->controller.clipboard_cs);
        char name[CS_NAME_SZ];
        ysw_name_create(name, sizeof(name));
        ysw_cs_set_name(cs, name);
        calculate_insert_index(csl);
        ysw_music_insert_cs(csl->controller.music, csl->controller.cs_index, cs);
        display_chord_styles(csl);
    } else {
        ysw_mb_clipboard_empty();
    }
}

static void on_trash_confirm(ysw_csl_t *csl)
{
    ysw_music_remove_cs(csl->controller.music, csl->controller.cs_index);
    display_chord_styles(csl);
}

static void on_trash(ysw_csl_t *csl, lv_obj_t *btn)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->controller.music);
    if (csl->controller.cs_index < cs_count) {
        ysw_cs_t *cs = ysw_music_get_cs(csl->controller.music, csl->controller.cs_index);
        char text[128];
        snprintf(text, sizeof(text), "Delete %s?", cs->name);
        ysw_mb_create_confirm(text, on_trash_confirm, csl);
    }
}

static void on_sort(ysw_csl_t *csl, lv_obj_t *btn)
{
    ysw_cs_t *cs = NULL;
    uint32_t cs_count = ysw_music_get_cs_count(csl->controller.music);
    if (csl->controller.cs_index < cs_count) {
        cs = ysw_music_get_cs(csl->controller.music, csl->controller.cs_index);
    }
    ysw_music_sort_cs_by_name(csl->controller.music);
    if (cs) {
        int32_t cs_index = ysw_music_get_cs_index(csl->controller.music, cs);
        if (cs_index != -1) {
            csl->controller.cs_index = cs_index;
        }
    }
    display_chord_styles(csl);
}

static void on_up(ysw_csl_t *csl, lv_obj_t *btn)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->controller.music);
    uint32_t new_index = csl->controller.cs_index - 1;
    if (csl->controller.cs_index < cs_count && new_index < cs_count) {
        ysw_array_swap(csl->controller.music->cs_array, csl->controller.cs_index, new_index);
        csl->controller.cs_index = new_index;
    }
    display_chord_styles(csl);
}

static void on_down(ysw_csl_t *csl, lv_obj_t *btn)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->controller.music);
    uint32_t new_index = csl->controller.cs_index + 1;
    if (new_index < cs_count) {
        ysw_array_swap(csl->controller.music->cs_array, csl->controller.cs_index, new_index);
        csl->controller.cs_index = new_index;
    }
    display_chord_styles(csl);
}

static void on_close(ysw_csl_t *csl, lv_obj_t *btn)
{
    ESP_LOGD(TAG, "on_close csl->controller.close_cb=%p", csl->controller.close_cb);
    ysw_seq_message_t message = {
            .type = YSW_SEQ_STOP,
    };
    ysw_main_seq_rendezvous(&message);
    if (csl->controller.close_cb) {
        csl->controller.close_cb(csl->controller.close_cb_context, csl);
    }
    ESP_LOGD(TAG, "on_close deleting csl->cont");
    lv_obj_del(csl->panel.container); // deletes contents
    ESP_LOGD(TAG, "on_close freeing csl");
    ysw_heap_free(csl);
}

static void on_next(ysw_csl_t *csl, lv_obj_t *btn)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->controller.music);
    if (csl->controller.cs_index < (cs_count - 1)) {
        csl->controller.cs_index++;
    }
    display_chord_styles(csl);
}

static void on_play(ysw_csl_t *csl, lv_obj_t *btn)
{
    ysw_cs_t *cs = ysw_music_get_cs(csl->controller.music, csl->controller.cs_index);
    ysw_cs_sort_sn_array(cs);

    uint32_t note_count = 0;
    ysw_note_t *notes = ysw_cs_get_notes(cs, &note_count);

    ysw_seq_message_t message = {
            .type = YSW_SEQ_PLAY,
            .stage.notes = notes,
            .stage.note_count = note_count,
            .stage.tempo = cs->tempo,
    };

    ysw_main_seq_send(&message);
}

static void on_stop(ysw_csl_t *csl, lv_obj_t *btn)
{
    ysw_seq_message_t message = {
            .type = YSW_SEQ_STOP,
    };

    ysw_main_seq_send(&message);
}

static void on_loop(ysw_csl_t *csl, lv_obj_t *btn)
{

    ESP_LOGD(TAG, "on_loop btn state=%d", lv_btn_get_state(btn));
    ysw_seq_message_t message = {
            .type = YSW_SEQ_LOOP,
            //.loop.loop = lv_btn_get_state(btn) == LV_BTN_STATE_RELEASED,
            .loop.loop = lv_btn_get_state(btn) == LV_BTN_STATE_CHECKED_RELEASED,
    };
    ysw_main_seq_send(&message);
}

static void on_prev(ysw_csl_t *csl, lv_obj_t *btn)
{
    if (csl->controller.cs_index > 0) {
        csl->controller.cs_index--;
    }
    display_chord_styles(csl);
}

ysw_csl_t* ysw_csl_create(lv_obj_t *parent, ysw_music_t *music, uint32_t cs_index)
{
    ysw_csl_t *csl = ysw_heap_allocate(sizeof(ysw_csl_t)); // freed in on_close

    csl->controller.music = music;
    csl->controller.cs_index = cs_index;

    ysw_ui_set_cbd(&csl->panel.header.prev.cbd, on_prev, csl);
    ysw_ui_set_cbd(&csl->panel.header.play.cbd, on_play, csl);
    ysw_ui_set_cbd(&csl->panel.header.stop.cbd, on_stop, csl);
    ysw_ui_set_cbd(&csl->panel.header.loop.cbd, on_loop, csl);
    ysw_ui_set_cbd(&csl->panel.header.next.cbd, on_next, csl);
    ysw_ui_set_cbd(&csl->panel.header.close.cbd, on_close, csl);
    ysw_ui_set_cbd(&csl->panel.footer.settings.cbd, on_settings, csl);
    ysw_ui_set_cbd(&csl->panel.footer.save.cbd, on_save, csl);
    ysw_ui_set_cbd(&csl->panel.footer.new.cbd, on_new, csl);
    ysw_ui_set_cbd(&csl->panel.footer.copy.cbd, on_copy, csl);
    ysw_ui_set_cbd(&csl->panel.footer.paste.cbd, on_paste, csl);
    ysw_ui_set_cbd(&csl->panel.footer.trash.cbd, on_trash, csl);
    ysw_ui_set_cbd(&csl->panel.footer.sort.cbd, on_sort, csl);
    ysw_ui_set_cbd(&csl->panel.footer.up.cbd, on_up, csl);
    ysw_ui_set_cbd(&csl->panel.footer.down.cbd, on_down, csl);

    csl->panel.container = lv_cont_create(parent, NULL);
    lv_obj_set_size(csl->panel.container, 320, 240);
    ysw_ui_adjust_styles(csl->panel.container);
    lv_obj_align_origo(csl->panel.container, parent, LV_ALIGN_CENTER, 0, 0);
    lv_cont_set_layout(csl->panel.container, LV_LAYOUT_COLUMN_MID);

    ysw_ui_create_header(csl->panel.container, &csl->panel.header);
    ysw_ui_create_body(csl->panel.container, &csl->panel.body);
    ysw_ui_create_footer(csl->panel.container, &csl->panel.footer);

    lv_label_set_text_static(csl->panel.footer.info.label, "");

    ysw_ui_distribute_extra_height(csl->panel.container, csl->panel.body.page);

    display_chord_styles(csl);

    return csl;
}

