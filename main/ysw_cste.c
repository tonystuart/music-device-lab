// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#if 0
// Example of getting component objects
// lv_win_ext_t *ext = lv_obj_get_ext_attr(win);
// lv_obj_t *scrl = lv_page_get_scrl(ext->page);

typedef struct {
    int16_t row;
    int16_t column;
    int16_t old_csn_index;
    bool is_cell_edit;
} selection_t;

static selection_t selection = {
    .row = -1,
    .old_csn_index = -1
};

static lv_obj_t *kb;
static lv_obj_t *win;
static lv_signal_cb_t old_signal_cb;

static lv_obj_t *table;
static lv_obj_t *footer_label;
static uint32_t csn_index;

static void keyboard_event_cb(lv_obj_t *keyboard, lv_event_t event)
{
    lv_kb_def_event_cb(kb, event);

    if(event == LV_EVENT_APPLY || event == LV_EVENT_CANCEL) {
        lv_obj_t *ta = lv_kb_get_ta(kb);
        if (ta) {
            lv_ta_set_cursor_type(ta, LV_CURSOR_LINE|LV_CURSOR_HIDDEN);
        }

        lv_obj_del(kb);
        kb = NULL;
    }
}

static void text_area_event_handler(lv_obj_t *ta, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        lv_obj_t *container = lv_obj_get_parent(ta);
        lv_obj_t *page = lv_obj_get_parent(container);
        lv_cont_set_layout(container, LV_LAYOUT_OFF);
        if (kb == NULL) {
            kb = lv_kb_create(page, NULL);
            lv_obj_set_width(kb, 290);
            lv_obj_set_event_cb(kb, keyboard_event_cb);
            lv_kb_set_cursor_manage(kb, true);
        }
        lv_kb_set_ta(kb, ta);
        lv_ll_move_before(&container->child_ll, kb, ta);
        lv_cont_set_layout(container, LV_LAYOUT_PRETTY);
    }
}

static void create_field(lv_obj_t *parent, char *name, char *value)
{
    lv_obj_t *name_label = lv_label_create(parent, NULL);
    lv_label_set_long_mode(name_label, LV_LABEL_LONG_BREAK);
    lv_label_set_text(name_label, name);
    lv_label_set_align(name_label, LV_LABEL_ALIGN_RIGHT);
    lv_obj_set_width(name_label, 75);

    lv_obj_t *value_ta = lv_ta_create(parent, NULL);
    lv_obj_set_width(value_ta, 200);
    lv_obj_set_protect(value_ta, LV_PROTECT_FOLLOW);
    lv_ta_set_style(value_ta, LV_TA_STYLE_BG, &value_cell);
    lv_ta_set_one_line(value_ta, true);
    lv_ta_set_cursor_type(value_ta, LV_CURSOR_LINE|LV_CURSOR_HIDDEN);
    lv_ta_set_text(value_ta, value);

    lv_obj_set_event_cb(value_ta, text_area_event_handler);
}

static void open_value_editor(lv_obj_t * btn, lv_event_t event)
{
    if(event != LV_EVENT_RELEASED) {
        return;
    }

    uint32_t cs_count = ysw_music_get_cs_count(music);
    if (cs_index >= cs_count) {
        return;
    }

    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);

    uint32_t csn_count = ysw_cs_get_csn_count(cs);
    if (csn_index >= csn_count) {
        return;
    }

    char buf[MDBUF_SZ];
    snprintf(buf, MDBUF_SZ, "Note (%d of %d)", to_count(csn_index), csn_count);

    ysw_csn_t *csn = ysw_cs_get_csn(cs, csn_index);

    lv_obj_t *win = lv_win_create(lv_scr_act(), NULL);
    lv_obj_align(win, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_win_set_style(win, LV_WIN_STYLE_BG, &lv_style_pretty);
    lv_win_set_style(win, LV_WIN_STYLE_CONTENT, &win_style_content);
    lv_win_set_title(win, buf);
    lv_win_set_layout(win, LV_LAYOUT_OFF);

    lv_obj_t *close_btn = lv_win_add_btn(win, LV_SYMBOL_CLOSE);
    lv_obj_set_event_cb(close_btn, lv_win_close_event_cb);

    lv_win_add_btn(win, LV_SYMBOL_SETTINGS);
    lv_win_set_btn_size(win, 20);

    //lv_win_ext_t *ext = lv_obj_get_ext_attr(win);
    //lv_obj_t *scrl = lv_page_get_scrl(ext->page);


    create_field(win, "Degree:", ysw_itoa(csn->degree, buf, MDBUF_SZ));
    create_field(win, "Start:", ysw_itoa(csn->start, buf, MDBUF_SZ));
    create_field(win, "Duration:", ysw_itoa(csn->duration, buf, MDBUF_SZ));
    create_field(win, "Volume:", ysw_itoa(csn->velocity, buf, MDBUF_SZ));

    lv_win_set_layout(win, LV_LAYOUT_PRETTY);
}

static char *headings[] = {
    "Degree", "Start", "Duration", "Volume"
};

#define COLUMN_COUNT (sizeof(headings) / sizeof(char*))

static void log_type(lv_obj_t *obj, char *tag)
{
    lv_obj_type_t types;
    lv_obj_get_type(obj, &types);

    uint8_t i;
    for(i = 0; i < LV_MAX_ANCESTOR_NUM; i++) {
        if (types.type[i]) {
            ESP_LOGD(tag, "types.type[%d]=%s", i, types.type[i]);
        }
    }
}

static void clear_selected_csn_highlight()
{
    if (selection.old_csn_index != -1) {
        for (int i = 0; i < COLUMN_COUNT; i++) {
            lv_table_set_cell_type(table, selection.old_csn_index + 1, i, 1); // +1 for heading
        }
    }
    selection.old_csn_index = -1;
    lv_obj_refresh_style(table);
}

static void select_csn()
{
    uint32_t cs_count = ysw_music_get_cs_count(music);
    if (cs_index >= cs_count) {
        return;
    }

    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);

    uint32_t csn_count = ysw_cs_get_csn_count(cs);
    if (csn_index >= csn_count) {
        return;
    }

    char buf[MDBUF_SZ];
    snprintf(buf, MDBUF_SZ, "Note %d of %d", to_count(csn_index), csn_count);
    lv_label_set_text(footer_label, buf);
    lv_obj_realign(footer_label);

    clear_selected_csn_highlight();
    for (int i = 0; i < COLUMN_COUNT; i++) {
        lv_table_set_cell_type(table, csn_index + 1, i, 4); // +1 for heading
    }
    lv_obj_refresh_style(table);

    selection.old_csn_index = csn_index;
}

static void select_cs()
{
    clear_selected_csn_highlight();

    uint32_t cs_count = ysw_music_get_cs_count(music);
    if (cs_index >= cs_count) {
        return;
    }

    ysw_cs_t *cs = ysw_music_get_cs(music, cs_index);

    lv_win_set_title(win, cs->name);

    uint32_t csn_count = ysw_cs_get_csn_count(cs);
    lv_table_set_row_cnt(table, csn_count + 1); // +1 for the headings

    for (uint32_t i = 0; i < csn_count; i++) {
        ESP_LOGD(TAG, "setting note attributes, i=%d", i);
        char buffer[16];
        int n = i + 1;
        ysw_csn_t *csn = ysw_cs_get_csn(cs, i); lv_table_set_cell_value(table, n, 0, ysw_itoa(csn->degree, buffer, sizeof(buffer)));
        lv_table_set_cell_value(table, n, 1, ysw_itoa(csn->start, buffer, sizeof(buffer)));
        lv_table_set_cell_value(table, n, 2, ysw_itoa(csn->duration, buffer, sizeof(buffer)));
        lv_table_set_cell_value(table, n, 3, ysw_itoa(csn->velocity, buffer, sizeof(buffer)));
        for (int j = 0; j < COLUMN_COUNT; j++) {
            lv_table_set_cell_align(table, n, j, LV_LABEL_ALIGN_CENTER);
        }
    }

    if (csn_count) {
        csn_index = 0;
        select_csn();
    }
}

static lv_res_t my_scrl_signal_cb(lv_obj_t *scrl, lv_signal_t sign, void *param)
{
    //ESP_LOGD(TAG, "my_scrl_signal_cb sign=%d", sign);
    if (sign == LV_SIGNAL_PRESSED) {
        lv_indev_t *indev_act = (lv_indev_t*)param;
        lv_indev_proc_t *proc = &indev_act->proc;
        lv_area_t scrl_coords;
        lv_obj_get_coords(scrl, &scrl_coords);
        uint16_t row = (proc->types.pointer.act_point.y + -scrl_coords.y1) / 30;
        uint16_t column = proc->types.pointer.act_point.x / 79;
        ESP_LOGD(TAG, "act_point x=%d, y=%d, scrl_coords x1=%d, y1=%d, x2=%d, y2=%d, row+1=%d, column+1=%d", proc->types.pointer.act_point.x, proc->types.pointer.act_point.y, scrl_coords.x1, scrl_coords.y1, scrl_coords.x2, scrl_coords.y2, row+1, column+1);

        // TODO: use a metdata structure to hold info about the cells
        if (row) {
            if (selection.is_cell_edit) {
                lv_table_set_cell_type(table, selection.row, selection.column, 1);
                lv_obj_refresh_style(table);
                selection.is_cell_edit = false;
                selection.row = -1;
            }
            csn_index = row - 1; // -1 for headings
            select_csn();
            if (selection.row == row && selection.column == column) {
                lv_table_set_cell_type(table, selection.row, selection.column, 2);
                lv_obj_refresh_style(table);
                selection.is_cell_edit = true;
            } else {
                selection.row = row;
                selection.column = column;
            }
        }
    }
    return old_signal_cb(scrl, sign, param);
}


static void fragment()
{
    lv_obj_t *scroller = lv_page_get_scrl(page);
    old_signal_cb = lv_obj_get_signal_cb(scroller);
    lv_obj_set_signal_cb(scroller, my_scrl_signal_cb);

    table = lv_table_create(win, NULL);

    lv_obj_align(table, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

    lv_table_set_style(table, LV_TABLE_STYLE_BG, &plain_color_tight);
    lv_table_set_style(table, LV_TABLE_STYLE_CELL1, &value_cell);
    lv_table_set_style(table, LV_TABLE_STYLE_CELL2, &cell_editor_style);
    lv_table_set_style(table, LV_TABLE_STYLE_CELL3, &name_cell);
    lv_table_set_style(table, LV_TABLE_STYLE_CELL4, &selected_cell);

    lv_table_set_row_cnt(table, 1); // just the headings for now
    lv_table_set_col_cnt(table, COLUMN_COUNT);

    for (int i = 0; i < COLUMN_COUNT; i++) {
        ESP_LOGD(TAG, "setting column attributes");
        lv_table_set_cell_type(table, 0, i, 3);
        lv_table_set_cell_align(table, 0, i, LV_LABEL_ALIGN_CENTER);
        lv_table_set_cell_value(table, 0, i, headings[i]);
        lv_table_set_col_width(table, i, 79);
    }

    cs_index = 0;
    select_cs();
}

static void select_chord_style()
{
    char buf[MDBUF_SZ];
    snprintf(buf, MDBUF_SZ, "Note %d of %d", to_count(csn_index), csn_count);
    lv_label_set_text(footer_label, buf);
    lv_obj_realign(footer_label);
}

#endif
