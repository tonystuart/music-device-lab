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
typedef void (*cb_t)(ysw_csl_t *csl, lv_obj_t *btn);
typedef void (*csl_close_cb_t)(void *context, ysw_csl_t *csl);

typedef struct {
    cb_t cb;
    void *context;
} cbd_t;

typedef struct {
    lv_obj_t *label;
} label_t;

typedef struct {
    lv_obj_t *btn;
    cbd_t cbd;
} button_t;

typedef struct {
    lv_obj_t *cont;
    label_t title;
    button_t prev;
    button_t play;
    button_t stop;
    button_t loop;
    button_t next;
    button_t close;
} header_t;

typedef struct {
    lv_obj_t *cont;
    button_t settings;
    button_t save;
    button_t new;
    button_t copy;
    button_t paste;
    button_t trash;
    button_t sort;
    button_t up;
    button_t down;
    label_t info;
} footer_t;

typedef struct {
    lv_obj_t *page;
} body_t;

typedef struct{
    ysw_music_t *music;
    uint32_t cs_index;
    ysw_cs_t *clipboard_cs;
    csl_close_cb_t close_cb;
    void *close_cb_context;
} ysw_csl_controller_t;

typedef struct  ysw_csl_s {
    lv_obj_t *cont;
    header_t header;
    body_t body;
    footer_t footer;
    ysw_csl_controller_t controller;
} ysw_csl_t;

typedef struct {
} config_t;

static void adjust_styles(lv_obj_t *obj)
{
    ysw_ui_lighten_background(obj);
    ysw_ui_clear_border(obj);
}

#if 0
static void ensure_visible(ysw_csl_t *csl, uint32_t row)
{
    lv_obj_t *page = lv_win_get_content(csl->frame->win);
    lv_obj_t *scrl = lv_page_get_scrl(page);

    lv_coord_t row_top = row * ROW_HEIGHT;
    lv_coord_t row_bottom = (row + 1) * ROW_HEIGHT;

    lv_coord_t viewport_height = lv_obj_get_height(page);

    lv_coord_t scrl_top = lv_obj_get_y(scrl); // always <= 0

    lv_coord_t visible_top = -scrl_top;
    lv_coord_t visible_bottom = visible_top + viewport_height;

    if (row_top < visible_top) {
        scrl_top = -row_top;
        ESP_LOGD(TAG, "setting scrl_top=%d", scrl_top);
        lv_obj_set_y(scrl, scrl_top);
    } else if (row_bottom > visible_bottom) {
        scrl_top = -row_top + (viewport_height - ROW_HEIGHT);
        ESP_LOGD(TAG, "setting scrl_top=%d", scrl_top);
        lv_obj_set_y(scrl, scrl_top);
    }
}
#endif

#if 0
#define MINIMUM_DRAG 5

static lv_res_t scrl_signal_cb(lv_obj_t *scrl, lv_signal_t signal, void *param)
{
    ysw_csl_t *csl = lv_obj_get_user_data(scrl);
    if (signal == LV_SIGNAL_PRESSED) {
        csl->dragged = false;
        lv_indev_t *indev_act = (lv_indev_t*)param;
        lv_indev_proc_t *proc = &indev_act->proc;
        csl->click_point = proc->types.pointer.act_point;
    } else if (signal == LV_SIGNAL_RELEASED) {
        if (!csl->dragged) {
            uint16_t row = get_row(scrl, param);
            if (row) {
                move_selection(csl, to_cs_index(row));
            }
        }
    } else if (signal == LV_SIGNAL_LONG_PRESS) {
        if (!csl->dragged) {
            lv_indev_wait_release((lv_indev_t *)param);
            uint16_t row = get_row(scrl, param);
            if (row) {
                edit_cs(csl, to_cs_index(row));
            }
        }
    } else if (signal == LV_SIGNAL_PRESSING) {
        if (!csl->dragged) {
            lv_indev_t *indev_act = (lv_indev_t*)param;
            lv_indev_proc_t *proc = &indev_act->proc;
            lv_point_t *point = &proc->types.pointer.act_point;
            lv_coord_t y = point->y - csl->click_point.y;
            csl->dragged = abs(y) > MINIMUM_DRAG;
        }
    }
    return prev_scrl_signal_cb(scrl, signal, param);
}
#endif

static void display_rows(ysw_csl_t *csl)
{
    lv_label_set_text_static(csl->footer.info.label, "");
    uint32_t cs_count = ysw_music_get_cs_count(csl->controller.music);
    lv_page_clean(csl->body.page);
    if (cs_count) {
        lv_page_set_scrl_layout(csl->body.page, LV_LAYOUT_OFF);
        for (uint32_t i = 0; i < cs_count; i++) {
            // TODO: consider factoring out function(s)
            ysw_cs_t *cs = ysw_music_get_cs(csl->controller.music, i);
            lv_obj_t *obj = lv_obj_create(csl->body.page, NULL);
            lv_obj_set_size(obj, 300, 34);
            adjust_styles(obj);
            lv_obj_t *label = lv_label_create(obj, NULL);
            lv_obj_align(label, obj, LV_ALIGN_IN_LEFT_MID, 5, 0);
            lv_label_set_text_static(label, cs->name);
            lv_obj_t *csp = ysw_csp_create(obj);
            lv_obj_align(csp, obj, LV_ALIGN_IN_RIGHT_MID, -2, 0);
            ysw_csp_set_cs(csp, cs);
        }
        lv_page_set_scrl_layout(csl->body.page, LV_LAYOUT_COLUMN_MID);
        if (csl->controller.cs_index >= cs_count) {
            csl->controller.cs_index = cs_count - 1;
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "%d of %d", csl->controller.cs_index + 1, cs_count);
        lv_label_set_text(csl->footer.info.label, buf);
    } else {
        csl->controller.cs_index = 0;
    }
}

static void on_btn_event(lv_obj_t *btn, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        cbd_t *cbd = lv_obj_get_user_data(btn);
        if (cbd) {
            cbd->cb(cbd->context, btn);
        }
    }
}

void set_cbd(cbd_t *cbd, cb_t cb, void *context)
{
    cbd->cb = cb;
    cbd->context = context;
}

void create_label(lv_obj_t *parent, label_t *label)
{
    label->label = lv_label_create(parent, NULL);
    lv_obj_set_size(label->label, 0, 30);
    lv_label_set_long_mode(label->label, LV_LABEL_LONG_CROP);
}

void create_button(lv_obj_t *parent, button_t *button, const void *img_src)
{
    button->btn = lv_btn_create(parent, NULL);
    lv_obj_set_size(button->btn, 20, 20);
    adjust_styles(button->btn);
    lv_obj_t *img = lv_img_create(button->btn, NULL);
    lv_obj_set_click(img, false);
    lv_img_set_src(img, img_src);
    lv_obj_set_user_data(button->btn, &button->cbd);
    lv_obj_set_event_cb(button->btn, on_btn_event);
}

static void create_header(lv_obj_t *parent, header_t *header)
{
    header->cont = lv_cont_create(parent, NULL);
    lv_obj_set_size(header->cont, 310, 30);
    adjust_styles(header->cont);
    lv_cont_set_layout(header->cont, LV_LAYOUT_ROW_MID); // TODO: move to bottom

    create_label(header->cont, &header->title);
    create_button(header->cont, &header->prev, LV_SYMBOL_PREV);
    create_button(header->cont, &header->play, LV_SYMBOL_PLAY);
    create_button(header->cont, &header->stop, LV_SYMBOL_STOP);
    create_button(header->cont, &header->loop, LV_SYMBOL_LOOP);
    create_button(header->cont, &header->next, LV_SYMBOL_NEXT);
    create_button(header->cont, &header->close, LV_SYMBOL_CLOSE);

    ysw_ui_distribute_extra_width(header->cont, header->title.label);
}

static void create_body(lv_obj_t *parent, body_t *body)
{
    body->page = lv_page_create(parent, NULL);
    lv_obj_set_size(body->page, 310, 0);
    adjust_styles(body->page);
    adjust_styles(lv_page_get_scrl(body->page));
    lv_page_set_scrl_layout(body->page, LV_LAYOUT_COLUMN_MID);
}

static void create_footer(lv_obj_t *parent, footer_t *footer)
{
    footer->cont = lv_cont_create(parent, NULL);
    lv_obj_set_size(footer->cont, 310, 30);
    adjust_styles(footer->cont);
    lv_cont_set_layout(footer->cont, LV_LAYOUT_ROW_MID); // TODO: move to bottom

    create_button(footer->cont, &footer->settings, LV_SYMBOL_SETTINGS);
    create_button(footer->cont, &footer->save, LV_SYMBOL_SAVE);
    create_button(footer->cont, &footer->new, LV_SYMBOL_AUDIO); // TODO: add NEW
    create_button(footer->cont, &footer->copy, LV_SYMBOL_COPY);
    create_button(footer->cont, &footer->paste, LV_SYMBOL_PASTE);
    create_button(footer->cont, &footer->trash, LV_SYMBOL_TRASH);
    create_button(footer->cont, &footer->sort, LV_SYMBOL_GPS); // TODO: add SORT
    create_button(footer->cont, &footer->up, LV_SYMBOL_UP);
    create_button(footer->cont, &footer->down, LV_SYMBOL_DOWN);
    create_label(footer->cont, &footer->info);
    lv_label_set_align(footer->info.label, LV_LABEL_ALIGN_RIGHT);

    ysw_ui_distribute_extra_width(footer->cont, footer->info.label);
}

static void on_csc_close(ysw_csl_t *csl, ysw_csc_t *csc)
{
    csl->controller.cs_index = csc->cs_index;
    display_rows(csl);
}

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
        display_rows(csl);
    } else {
        ysw_mb_clipboard_empty();
    }
}

static void on_trash_confirm(ysw_csl_t *csl)
{
    ysw_music_remove_cs(csl->controller.music, csl->controller.cs_index);
    display_rows(csl);
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
    ysw_music_sort_cs_by_name(csl->controller.music);
    display_rows(csl);
}

static void on_up(ysw_csl_t *csl, lv_obj_t *btn)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->controller.music);
    uint32_t new_index = csl->controller.cs_index - 1;
    if (csl->controller.cs_index < cs_count && new_index < cs_count) {
        ysw_array_swap(csl->controller.music->cs_array, csl->controller.cs_index, new_index);
        csl->controller.cs_index = new_index;
    }
    display_rows(csl);
}

static void on_down(ysw_csl_t *csl, lv_obj_t *btn)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->controller.music);
    uint32_t new_index = csl->controller.cs_index + 1;
    if (new_index < cs_count) {
        ysw_array_swap(csl->controller.music->cs_array, csl->controller.cs_index, new_index);
        csl->controller.cs_index = new_index;
    }
    display_rows(csl);
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
    lv_obj_del(csl->cont); // deletes contents
    ESP_LOGD(TAG, "on_close freeing csl");
    ysw_heap_free(csl);
}

static void on_next(ysw_csl_t *csl, lv_obj_t *btn)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->controller.music);
    if (csl->controller.cs_index < (cs_count - 1)) {
        csl->controller.cs_index++;
    }
    display_rows(csl);
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
    display_rows(csl);
}

ysw_csl_t* ysw_csl_create(lv_obj_t *parent, ysw_music_t *music, uint32_t cs_index)
{
    ysw_csl_t *csl = ysw_heap_allocate(sizeof(ysw_csl_t)); // freed in on_close

    csl->controller.music = music;
    csl->controller.cs_index = cs_index;

    set_cbd(&csl->header.prev.cbd, on_prev, csl);
    set_cbd(&csl->header.play.cbd, on_play, csl);
    set_cbd(&csl->header.stop.cbd, on_stop, csl);
    set_cbd(&csl->header.loop.cbd, on_loop, csl);
    set_cbd(&csl->header.next.cbd, on_next, csl);
    set_cbd(&csl->header.close.cbd, on_close, csl);
    set_cbd(&csl->footer.settings.cbd, on_settings, csl);
    set_cbd(&csl->footer.save.cbd, on_save, csl);
    set_cbd(&csl->footer.new.cbd, on_new, csl);
    set_cbd(&csl->footer.copy.cbd, on_copy, csl);
    set_cbd(&csl->footer.paste.cbd, on_paste, csl);
    set_cbd(&csl->footer.trash.cbd, on_trash, csl);
    set_cbd(&csl->footer.sort.cbd, on_sort, csl);
    set_cbd(&csl->footer.up.cbd, on_up, csl);
    set_cbd(&csl->footer.down.cbd, on_down, csl);

    csl->cont = lv_cont_create(parent, NULL);
    lv_obj_set_size(csl->cont, 320, 240);
    adjust_styles(csl->cont);
    lv_obj_align_origo(csl->cont, parent, LV_ALIGN_CENTER, 0, 0);
    lv_cont_set_layout(csl->cont, LV_LAYOUT_COLUMN_MID);

    create_header(csl->cont, &csl->header);
    create_body(csl->cont, &csl->body);
    create_footer(csl->cont, &csl->footer);

    lv_label_set_text_static(csl->header.title.label, "Chord Styles");
    lv_label_set_text_static(csl->footer.info.label, "");

    ysw_ui_distribute_extra_height(csl->cont, csl->body.page);

    display_rows(csl);

    return csl;
}

