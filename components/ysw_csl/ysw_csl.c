// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_csl.h"

#include "ysw_auto_play.h"
#include "ysw_cs.h"
#include "ysw_lv_cse.h"
#include "ysw_mb.h"
#include "ysw_mfw.h"
#include "ysw_name.h"
#include "ysw_sdb.h"
#include "ysw_sn.h"
#include "ysw_heap.h"
#include "ysw_lv_styles.h"

#include "../../main/csc.h" // TODO: move csc to components or this to main
#include "../../main/seq.h"

#include "lvgl.h"

#include "esp_log.h"

#include "math.h"
#include "stdio.h"

#define TAG "YSW_CSL"

#define ROW_HEIGHT 30 // TODO: calculate this

#define YSW_CSL_WHITE LV_TABLE_STYLE_CELL1
#define YSW_CSL_GRAY LV_TABLE_STYLE_CELL2
#define YSW_CSL_YELLOW LV_TABLE_STYLE_CELL3

// See https://en.wikipedia.org/wiki/Chord_letters#Chord_quality

static lv_signal_cb_t prev_scrl_signal_cb;

typedef struct {
    const char *name;
    uint8_t width;
} headings_t;

static const headings_t headings[] = {
    { "Chord Style", 159 },
    { "Divisions", 80 },
    { "Notes", 80 },
};

#define COLUMN_COUNT (sizeof(headings) / sizeof(headings_t))

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

static void hide_selected_cs(ysw_csl_t *csl)
{
    uint32_t row = csl->cs_index + 1; // +1 for headings
    for (int i = 0; i < COLUMN_COUNT; i++) {
        lv_table_set_cell_type(csl->table, row, i, YSW_CSL_WHITE);
    }
    ysw_frame_set_footer_text(csl->frame, "");
    lv_obj_refresh_style(csl->table);
}

static void show_selected_cs(ysw_csl_t *csl)
{
    uint32_t row = csl->cs_index + 1; // +1 for headings
    for (uint32_t i = 0; i < COLUMN_COUNT; i++) {
        lv_table_set_cell_type(csl->table, row, i, YSW_CSL_YELLOW);
    }
    char buf[64];
    uint32_t cs_count = ysw_music_get_cs_count(csl->music);
    snprintf(buf, sizeof(buf), "%d of %d", row, cs_count);
    ysw_frame_set_footer_text(csl->frame, buf);
    lv_obj_refresh_style(csl->table);
    ensure_visible(csl, row);
}

static void move_selection(ysw_csl_t *csl, uint32_t cs_index)
{
    hide_selected_cs(csl);
    csl->cs_index = cs_index;
    show_selected_cs(csl);
}

static void display_row(ysw_csl_t *csl, uint32_t i)
{
    char buffer[16];
    int row = i + 1; // +1 for headings
    ysw_cs_t *cs = ysw_music_get_cs(csl->music, i);
    uint32_t sn_count = ysw_cs_get_sn_count(cs);
    lv_table_set_cell_crop(csl->table, row, 0, true);
    lv_table_set_cell_value(csl->table, row, 0, cs->name);
    lv_table_set_cell_value(csl->table, row, 1, ysw_itoa(cs->divisions, buffer, sizeof(buffer)));
    lv_table_set_cell_value(csl->table, row, 2, ysw_itoa(sn_count, buffer, sizeof(buffer)));
    for (int j = 0; j < COLUMN_COUNT; j++) {
        lv_table_set_cell_align(csl->table, row, j, LV_LABEL_ALIGN_CENTER);
        lv_table_set_cell_type(csl->table, row, j, YSW_CSL_WHITE);
    }
}

static void display_rows(ysw_csl_t *csl)
{
    ysw_frame_set_footer_text(csl->frame, "");
    uint32_t cs_count = ysw_music_get_cs_count(csl->music);
    lv_table_set_row_cnt(csl->table, cs_count + 1); // +1 for headings
    if (cs_count) {
        for (uint32_t i = 0; i < cs_count; i++) {
            display_row(csl, i);
        }
        if (csl->cs_index >= cs_count) {
            csl->cs_index = cs_count - 1;
        }
        show_selected_cs(csl);
    } else {
        csl->cs_index = 0;
    }
}

static void on_csc_close(ysw_csl_t *csl, csc_t *csc)
{
    csl->cs_index = csc->cs_index;
    display_rows(csl);
}

static void edit_cs(ysw_csl_t *csl, uint32_t cs_index)
{
    csc_t *csc = csc_create(csl->music, cs_index);
    csc_set_close_cb(csc, on_csc_close, csl);
}

static void create_cs(ysw_csl_t *csl, uint32_t cs_index)
{
    csc_t *csc = csc_create_new(csl->music, cs_index);
    csc_set_close_cb(csc, on_csc_close, csl);
}

static uint32_t get_row(lv_obj_t *scrl, void *param)
{
    lv_area_t scrl_coords;
    lv_obj_get_coords(scrl, &scrl_coords);
    lv_indev_t *indev_act = (lv_indev_t *)param;
    lv_indev_proc_t *proc = &indev_act->proc;
    lv_coord_t y = proc->types.pointer.act_point.y + -scrl_coords.y1;
    uint32_t row = y / ROW_HEIGHT;
    return row;
}

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
                move_selection(csl, row - 1); // -1 for headings;
            }
        }
    } else if (signal == LV_SIGNAL_LONG_PRESS) {
        if (!csl->dragged) {
            lv_indev_wait_release((lv_indev_t *)param);
            uint16_t row = get_row(scrl, param);
            if (row) {
                edit_cs(csl, row - 1); // -1 for headings;
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

static void on_auto_play_all(ysw_csl_t *csl, ysw_auto_play_t auto_play)
{
    ysw_lv_cse_gs.auto_play_all = auto_play;
}

static void on_auto_play_last(ysw_csl_t *csl, ysw_auto_play_t auto_play)
{
    ysw_lv_cse_gs.auto_play_last = auto_play;
}

static void on_multiple_selection(ysw_csl_t *csl, bool multiple_selection)
{
    ysw_lv_cse_gs.multiple_selection = multiple_selection;
}

static void on_settings(ysw_csl_t *csl, lv_obj_t * btn)
{
    ysw_sdb_t *sdb = ysw_sdb_create("Chord Style Editor Settings", csl);
    ysw_sdb_add_choice(sdb, "Multiple Selection",
            ysw_lv_cse_gs.multiple_selection, "No\nYes", on_multiple_selection);
    ysw_sdb_add_separator(sdb, "Auto Play Settings");
    ysw_sdb_add_choice(sdb, "On Change",
            ysw_lv_cse_gs.auto_play_all, ysw_auto_play_options, on_auto_play_all);
    ysw_sdb_add_choice(sdb, "On Click",
            ysw_lv_cse_gs.auto_play_last, ysw_auto_play_options, on_auto_play_last);
}

static void on_save(ysw_csl_t *csl, lv_obj_t * btn)
{
    ysw_mfw_write(csl->music);
}

static void on_new(ysw_csl_t *csl, lv_obj_t * btn)
{
    create_cs(csl, csl->cs_index);
}

static void on_copy(ysw_csl_t *csl, lv_obj_t * btn)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->music);
    if (csl->cs_index < cs_count) {
        if (csl->clipboard_cs) {
            ysw_cs_free(csl->clipboard_cs);
        }
        ysw_cs_t *cs = ysw_music_get_cs(csl->music, csl->cs_index);
        csl->clipboard_cs = ysw_cs_copy(cs);
    } else {
        ysw_mb_nothing_selected();
    }
}

static void calculate_insert_index(ysw_csl_t *csl)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->music);
    if (csl->cs_index < cs_count) {
        csl->cs_index++; // insert afterwards
    } else {
        csl->cs_index = cs_count; // insert at end (or beginning if empty)
    }
}

static void on_paste(ysw_csl_t *csl, lv_obj_t * btn)
{
    if (csl->clipboard_cs) {
        ysw_cs_t *cs = ysw_cs_copy(csl->clipboard_cs);
        char name[CS_NAME_SZ];
        ysw_name_create(name, sizeof(name));
        ysw_cs_set_name(cs, name);
        calculate_insert_index(csl);
        ysw_music_insert_cs(csl->music, csl->cs_index, cs);
        display_rows(csl);
    } else {
        ysw_mb_clipboard_empty();
    }
}

static void on_trash_confirm(ysw_csl_t *csl)
{
    ysw_music_remove_cs(csl->music, csl->cs_index);
    display_rows(csl);
}

static void on_trash(ysw_csl_t *csl, lv_obj_t * btn)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->music);
    if (csl->cs_index < cs_count) {
        ysw_cs_t *cs = ysw_music_get_cs(csl->music, csl->cs_index);
        char text[128];
        snprintf(text, sizeof(text), "Delete %s?", cs->name);
        ysw_mb_create_confirm(text, on_trash_confirm, csl);
    }
}

static void on_sort(ysw_csl_t *csl, lv_obj_t * btn)
{
    ysw_music_sort_cs_by_name(csl->music);
    display_rows(csl);
}

static void on_up(ysw_csl_t *csl, lv_obj_t * btn)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->music);
    uint32_t new_index = csl->cs_index - 1;
    if (csl->cs_index < cs_count && new_index < cs_count) {
        ysw_array_swap(csl->music->cs_array, csl->cs_index, new_index);
        csl->cs_index = new_index;
    }
    display_rows(csl);
}

static void on_down(ysw_csl_t *csl, lv_obj_t * btn)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->music);
    uint32_t new_index = csl->cs_index + 1;
    if (new_index < cs_count) {
        ysw_array_swap(csl->music->cs_array, csl->cs_index, new_index);
        csl->cs_index = new_index;
    }
    display_rows(csl);
}

static void on_close(ysw_csl_t *csl, lv_obj_t * btn)
{
    ESP_LOGD(TAG, "on_close csl->close_cb=%p", csl->close_cb);
    ysw_seq_message_t message = {
        .type = YSW_SEQ_STOP,
    };
    seq_rendezvous(&message);
    if (csl->close_cb) {
        csl->close_cb(csl->close_cb_context, csl);
    }
    ESP_LOGD(TAG, "on_close deleting csl->frame");
    ysw_frame_del(csl->frame); // deletes contents
    ESP_LOGD(TAG, "on_close freeing csl");
    ysw_heap_free(csl);
}

static void on_next(ysw_csl_t *csl, lv_obj_t * btn)
{
    uint32_t cs_count = ysw_music_get_cs_count(csl->music);
    if (csl->cs_index < (cs_count - 1)) {
        csl->cs_index++;
    }
    display_rows(csl);
}

static void on_play(ysw_csl_t *csl, lv_obj_t * btn)
{
    ysw_cs_t *cs = ysw_music_get_cs(csl->music, csl->cs_index);
    ysw_cs_sort_sn_array(cs);

    uint32_t note_count = 0;
    ysw_note_t *notes = ysw_cs_get_notes(cs, &note_count);

    ysw_seq_message_t message = {
        .type = YSW_SEQ_PLAY,
        .stage.notes = notes,
        .stage.note_count = note_count,
        .stage.tempo = cs->tempo,
    };

    seq_send(&message);
}

static void on_stop(ysw_csl_t *csl, lv_obj_t * btn)
{
    ysw_seq_message_t message = {
        .type = YSW_SEQ_STOP,
    };

    seq_send(&message);
}

static void on_loop(ysw_csl_t *csl, lv_obj_t * btn)
{

    ESP_LOGD(TAG, "on_loop btn state=%d", lv_btn_get_state(btn));
    ysw_seq_message_t message = {
        .type = YSW_SEQ_LOOP,
        //.loop.loop = lv_btn_get_state(btn) == LV_BTN_STATE_REL,
        .loop.loop = lv_btn_get_state(btn) == LV_BTN_STATE_TGL_REL,
    };
    seq_send(&message);
}

static void on_prev(ysw_csl_t *csl, lv_obj_t * btn)
{
    if (csl->cs_index > 0) {
        csl->cs_index--;
    }
    display_rows(csl);
}

static ysw_frame_t *create_frame(ysw_csl_t *csl)
{   
    ysw_frame_t *frame = ysw_frame_create(csl);

    ysw_frame_add_footer_button(frame, LV_SYMBOL_SETTINGS, on_settings);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_SAVE, on_save);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_AUDIO, on_new);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_COPY, on_copy);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_PASTE, on_paste);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_TRASH, on_trash);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_GPS, on_sort); // TODO: add SORT symbol
    ysw_frame_add_footer_button(frame, LV_SYMBOL_UP, on_up);
    ysw_frame_add_footer_button(frame, LV_SYMBOL_DOWN, on_down);

    ysw_frame_add_header_button(frame, LV_SYMBOL_CLOSE, on_close);
    ysw_frame_add_header_button(frame, LV_SYMBOL_NEXT, on_next);
    seq_init_loop_btn(ysw_frame_add_header_button(frame, LV_SYMBOL_LOOP, on_loop));
    ysw_frame_add_header_button(frame, LV_SYMBOL_STOP, on_stop);
    ysw_frame_add_header_button(frame, LV_SYMBOL_PLAY, on_play);
    ysw_frame_add_header_button(frame, LV_SYMBOL_PREV, on_prev);

    return frame;
}

ysw_csl_t *ysw_csl_create(ysw_music_t *music)
{
    ysw_csl_t *csl = ysw_heap_allocate(sizeof(ysw_csl_t)); // freed in on_close

    csl->music = music;
    csl->frame = create_frame(csl);

    ysw_frame_set_header_text(csl->frame, "Chord Styles");

    csl->table = lv_table_create(csl->frame->win, NULL);
    lv_obj_set_user_data(csl->table, csl);

    lv_obj_align(csl->table, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

    lv_table_set_style(csl->table, LV_TABLE_STYLE_BG, &ysw_style_table_bg);
    lv_table_set_style(csl->table, YSW_CSL_GRAY, &ysw_style_gray_cell);
    lv_table_set_style(csl->table, YSW_CSL_WHITE, &ysw_style_white_cell);
    lv_table_set_style(csl->table, YSW_CSL_YELLOW, &ysw_style_yellow_cell);

    lv_table_set_row_cnt(csl->table, 1); // just headings for now
    lv_table_set_col_cnt(csl->table, COLUMN_COUNT);

    for (int i = 0; i < COLUMN_COUNT; i++) {
        lv_table_set_cell_type(csl->table, 0, i, YSW_CSL_GRAY);
        lv_table_set_cell_align(csl->table, 0, i, LV_LABEL_ALIGN_CENTER);
        lv_table_set_cell_value(csl->table, 0, i, headings[i].name);
        lv_table_set_col_width(csl->table, i, headings[i].width);
    }

    display_rows(csl);

    lv_obj_t *page = lv_win_get_content(csl->frame->win);
    lv_obj_t *scrl = lv_page_get_scrl(page);
    lv_obj_set_user_data(scrl, csl);
    if (!prev_scrl_signal_cb) {
        prev_scrl_signal_cb = lv_obj_get_signal_cb(scrl);
    }
    lv_obj_set_signal_cb(scrl, scrl_signal_cb);

    return csl;
}

