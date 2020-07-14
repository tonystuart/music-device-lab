// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_hpl.h"

#include "ysw_auto_play.h"
#include "ysw_hp.h"
#include "ysw_hpc.h"
#include "ysw_hpe.h"
#include "ysw_hpp.h"
#include "ysw_heap.h"
#include "ysw_main_seq.h"
#include "ysw_mb.h"
#include "ysw_mfw.h"
#include "ysw_music.h"
#include "ysw_name.h"
#include "ysw_sdb.h"
#include "ysw_ps.h" // TODO: remove if not needed (and ysw_sn in ysw_csl.c)
#include "ysw_style.h"
#include "ysw_ui.h"

#include "lvgl.h"
#include "esp_log.h"

#include "stdlib.h"

#define TAG "YSW_HPL"

static void on_row_event(lv_obj_t *obj, lv_event_t event);

static void clear_selection_highlight(ysw_hpl_t *hpl)
{
    uint32_t hp_count = ysw_music_get_hp_count(hpl->model.music);
    if (hpl->model.hp_index < hp_count) {
        lv_obj_t *scrl = lv_page_get_scrl(hpl->frame.body.page);
        lv_obj_t *child = ysw_ui_child_at_index(scrl, hpl->model.hp_index);
        if (child) {
            ysw_style_adjust_hpl_selection(child, false);
        }
    }
    ysw_ui_set_footer_text(&hpl->frame.footer, "");
}

static void display_selection_highlight(ysw_hpl_t *hpl)
{
    uint32_t hp_count = ysw_music_get_hp_count(hpl->model.music);
    if (hpl->model.hp_index < hp_count) {
        lv_obj_t *scrl = lv_page_get_scrl(hpl->frame.body.page);
        lv_obj_t *child = ysw_ui_child_at_index(scrl, hpl->model.hp_index);
        if (child) {
            ysw_style_adjust_hpl_selection(child, true);
            ysw_ui_ensure_visible(child, true);
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "%d of %d", hpl->model.hp_index + 1, hp_count);
        ysw_ui_set_footer_text(&hpl->frame.footer, buf);
    }
}

static void display_progressions(ysw_hpl_t *hpl)
{
    lv_label_set_text_static(hpl->frame.footer.info.label, "");
    uint32_t hp_count = ysw_music_get_hp_count(hpl->model.music);
    lv_page_clean(hpl->frame.body.page);
    if (hp_count) {
        lv_page_set_scrl_layout(hpl->frame.body.page, LV_LAYOUT_OFF);
        for (uint32_t i = 0; i < hp_count; i++) {
            // TODO: consider factoring out function(s)
            ysw_hp_t *hp = ysw_music_get_hp(hpl->model.music, i);
            lv_obj_t *obj = lv_obj_create(hpl->frame.body.page, NULL);
            lv_obj_set_size(obj, 300, 34);
            ysw_style_adjust_obj(obj);
            lv_obj_set_user_data(obj, hpl);
            lv_obj_t *label = lv_label_create(obj, NULL);
            lv_obj_align(label, obj, LV_ALIGN_IN_LEFT_MID, 5, 0);
            lv_label_set_text_static(label, hp->name);
            lv_obj_t *hpp = ysw_hpp_create(obj);
            lv_obj_align(hpp, obj, LV_ALIGN_IN_RIGHT_MID, -2, 0);
            ysw_hpp_set_hp(hpp, hp);
            lv_obj_set_event_cb(obj, on_row_event);
        }
        lv_page_set_scrl_layout(hpl->frame.body.page, LV_LAYOUT_COLUMN_MID);
        if (hpl->model.hp_index >= hp_count) {
            hpl->model.hp_index = hp_count - 1;
        }
        display_selection_highlight(hpl);
    } else {
        hpl->model.hp_index = 0;
        clear_selection_highlight(hpl);
    }
}

static void on_short_click(lv_obj_t *obj)
{
    ysw_hpl_t *hpl = lv_obj_get_user_data(obj);
    if (hpl) {
        uint32_t hp_index = ysw_ui_get_index_of_child(obj);
        if (hp_index != hpl->model.hp_index) {
            clear_selection_highlight(hpl);
            hpl->model.hp_index = hp_index;
            display_selection_highlight(hpl);
        }
    }
}

static void on_row_event(lv_obj_t *obj, lv_event_t event)
{
    char type[128];
    ysw_ui_get_obj_type(obj, type, sizeof(type));
    switch (event) {
        case LV_EVENT_PRESSED:
            break;
        case LV_EVENT_SHORT_CLICKED:
            on_short_click(obj);
            break;
        case LV_EVENT_CLICKED:
            break;
        case LV_EVENT_LONG_PRESSED:
            break;
        case LV_EVENT_LONG_PRESSED_REPEAT:
            break;
        case LV_EVENT_RELEASED:
            break;
    }
}

static void on_hpc_close(ysw_hpl_model_t *model, uint32_t hp_index)
{
    ysw_hpl_create(model->music, hp_index);
    ysw_heap_free(model);
}

static void launch_style_editor(ysw_hpl_t *hpl, ysw_hpc_type_t type)
{
    ysw_hpl_model_t *model = ysw_heap_allocate(sizeof(ysw_hpl_model_t));
    *model = hpl->model;
    ysw_hpl_close(hpl); // no ref to hpl after this point
    ysw_hpc_t *hpc = ysw_hpc_create(model->music, model->hp_index, type);
    ysw_hpc_set_close_cb(hpc, on_hpc_close, model);
}

static void on_auto_play_all(ysw_hpl_t *hpl, uint16_t auto_play)
{
    ysw_hpe_gs.auto_play_all = auto_play;
}

static void on_auto_play_last(ysw_hpl_t *hpl, uint16_t auto_play)
{
    ysw_hpe_gs.auto_play_last = auto_play;
}

static void on_multiple_selection(ysw_hpl_t *hpl, uint16_t multiple_selection)
{
    ysw_hpe_gs.multiple_selection = multiple_selection;
}

static void on_settings(ysw_hpl_t *hpl, lv_obj_t *btn)
{
    ysw_sdb_t *sdb = ysw_sdb_create_standard("Chord Style Editor Settings", hpl);

    ysw_sdb_add_choice(sdb, "Multiple Selection:",
            ysw_hpe_gs.multiple_selection, "No\nYes", on_multiple_selection);

    ysw_sdb_add_choice(sdb, "Auto Play - On Change:",
            ysw_hpe_gs.auto_play_all, ysw_auto_play_options, on_auto_play_all);

    ysw_sdb_add_choice(sdb, "Auto Play - On Click:",
            ysw_hpe_gs.auto_play_last, ysw_auto_play_options, on_auto_play_last);
}

static void on_save(ysw_hpl_t *hpl, lv_obj_t *btn)
{
    ysw_mfw_write(hpl->model.music);
}

static void on_new(ysw_hpl_t *hpl, lv_obj_t *btn)
{
    launch_style_editor(hpl, YSW_HPC_CREATE_HP);
}

static void on_copy(ysw_hpl_t *hpl, lv_obj_t *btn)
{
    uint32_t hp_count = ysw_music_get_hp_count(hpl->model.music);
    if (hpl->model.hp_index < hp_count) {
        if (hpl->controller.clipboard_hp) {
            ysw_hp_free(hpl->controller.clipboard_hp);
        }
        ysw_hp_t *hp = ysw_music_get_hp(hpl->model.music, hpl->model.hp_index);
        hpl->controller.clipboard_hp = ysw_hp_copy(hp);
    } else {
        ysw_mb_nothing_selected();
    }
}

static void find_unique_name(ysw_music_t *music, const char *old_name, char *new_name, uint32_t size)
{
    uint32_t max_version = 0;
    uint32_t hp_count = ysw_music_get_hp_count(music);
    uint32_t version_point = ysw_name_find_version_point(old_name);
    for (uint32_t i = 0; i < hp_count; i++) {
        ysw_hp_t *hp = ysw_music_get_hp(music, i);
        if (strncmp(hp->name, old_name, version_point) == 0) {
            max_version = max(max_version, atoi(hp->name + version_point) + 1);
        }
    }
    ysw_name_format_version(new_name, size, version_point, old_name, max_version);
}

static void on_paste(ysw_hpl_t *hpl, lv_obj_t *btn)
{
    if (hpl->controller.clipboard_hp) {
        ysw_hp_t *hp = ysw_hp_copy(hpl->controller.clipboard_hp);
        char new_name[HP_NAME_SZ];
        find_unique_name(hpl->model.music, hp->name, new_name, sizeof(new_name));
        ysw_hp_set_name(hp, new_name);
        // TODO: use alphabetized list
        //hpl->model.hp_index = ysw_music_insert_hp(hpl->model.music, hp);
        display_progressions(hpl);
    } else {
        ysw_mb_clipboard_empty();
    }
}

static void on_trash_confirm(ysw_hpl_t *hpl)
{
    // TODO: implement remove
    //ysw_music_remove_hp(hpl->model.music, hpl->model.hp_index);
    display_progressions(hpl);
}

static void on_trash(ysw_hpl_t *hpl, lv_obj_t *btn)
{
    uint32_t hp_count = ysw_music_get_hp_count(hpl->model.music);
    if (hpl->model.hp_index < hp_count) {
        ysw_hp_t *hp = ysw_music_get_hp(hpl->model.music, hpl->model.hp_index);
        char text[128];
        snprintf(text, sizeof(text), "Delete %s?", hp->name);
        ysw_mb_create_confirm(text, on_trash_confirm, hpl);
    }
}

static void on_edit(ysw_hpl_t *hpl, lv_obj_t *btn)
{
    launch_style_editor(hpl, YSW_HPC_EDIT_HP);
}

static void on_next(ysw_hpl_t *hpl, lv_obj_t *btn)
{
    uint32_t hp_count = ysw_music_get_hp_count(hpl->model.music);
    if (hpl->model.hp_index < (hp_count - 1)) {
        hpl->model.hp_index++;
    }
    display_progressions(hpl);
}

static void on_play(ysw_hpl_t *hpl, lv_obj_t *btn)
{
    ysw_hp_t *hp = ysw_music_get_hp(hpl->model.music, hpl->model.hp_index);

    uint32_t note_count = 0;
    ysw_note_t *notes = ysw_hp_get_notes(hp, &note_count);

    ysw_seq_message_t message = {
        .type = YSW_SEQ_PLAY,
        .stage.notes = notes,
        .stage.note_count = note_count,
        .stage.tempo = hp->tempo,
    };

    ysw_main_seq_send(&message);
}

static void on_prev(ysw_hpl_t *hpl, lv_obj_t *btn)
{
    if (hpl->model.hp_index > 0) {
        hpl->model.hp_index--;
    }
    display_progressions(hpl);
}

static void on_close(ysw_hpl_t *hpl, lv_obj_t *btn)
{
    ysw_hpl_close(hpl);
}

static const ysw_ui_btn_def_t header_buttons[] = {
    { LV_SYMBOL_PREV, on_prev },
    { LV_SYMBOL_PLAY, on_play },
    { LV_SYMBOL_STOP, ysw_main_seq_on_stop },
    { LV_SYMBOL_LOOP, ysw_main_seq_on_loop },
    { LV_SYMBOL_NEXT, on_next },
    { LV_SYMBOL_CLOSE, on_close },
    { NULL, NULL },
};

static const ysw_ui_btn_def_t footer_buttons[] = {
    { LV_SYMBOL_SETTINGS, on_settings },
    { LV_SYMBOL_SAVE, on_save },
    { LV_SYMBOL_AUDIO, on_new }, // TODO: add NEW button
    { LV_SYMBOL_COPY, on_copy },
    { LV_SYMBOL_PASTE, on_paste },
    { LV_SYMBOL_TRASH, on_trash },
    { LV_SYMBOL_EDIT, on_edit },
    { NULL, NULL },
};

ysw_hpl_t* ysw_hpl_create(ysw_music_t *music, uint32_t hp_index)
{
    ysw_hpl_t *hpl = ysw_heap_allocate(sizeof(ysw_hpl_t)); // freed in on_close

    hpl->model.music = music;
    hpl->model.hp_index = hp_index;

    ysw_ui_init_buttons(hpl->frame.header.buttons, header_buttons, hpl);
    ysw_ui_init_buttons(hpl->frame.footer.buttons, footer_buttons, hpl);
    ysw_ui_create_frame(&hpl->frame);
    ysw_ui_set_header_text(&hpl->frame.header, "Chord Styles");
    display_progressions(hpl);
    return hpl;
}

void ysw_hpl_close(ysw_hpl_t *hpl)
{
    ysw_seq_message_t message = {
        .type = YSW_SEQ_STOP,
    };
    ysw_main_seq_rendezvous(&message);
    lv_obj_del(hpl->frame.container); // deletes contents
    if (hpl->controller.clipboard_hp) {
        ysw_hp_free(hpl->controller.clipboard_hp);
    }
    ysw_heap_free(hpl);
}

