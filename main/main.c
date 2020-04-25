// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "esp_log.h"
#include "esp_spiffs.h"

#include "lvgl/lvgl.h"
#include "driver/spi_master.h"
#include "eli_ili9341_xpt2046.h"

#include "ysw_synthesizer.h"
#include "ysw_ble_synthesizer.h"
#include "ysw_sequencer.h"
#include "ysw_message.h"
#include "ysw_chord.h"
#include "ysw_music.h"
#include "ysw_music_parser.h"
#include "ysw_lv_styles.h"

#define TAG "MAIN"

#define SPIFFS_PARTITION "/spiffs"
#define MUSIC_DEFINITIONS "/spiffs/music.csv"

#define BUTTON_SIZE 20

// Example of getting component objects
// lv_win_ext_t *ext = lv_obj_get_ext_attr(win);
// lv_obj_t *scrl = lv_page_get_scrl(ext->page);

typedef struct {
    int16_t row;
    int16_t column;
    int16_t old_note_index;
    bool is_cell_edit;
} selection_t;

static selection_t selection = {
    .row = -1,
    .old_note_index = -1
};

static lv_obj_t *kb;
static lv_obj_t *table;
static lv_signal_cb_t old_signal_cb;

static QueueHandle_t synthesizer_queue;
static QueueHandle_t sequencer_queue;

static ysw_music_t *music;
static uint32_t progression_index;

static void play_progression(ysw_progression_t *s);

static uint32_t chord_index;
static uint32_t note_index;
static lv_obj_t *footer_label;
static lv_obj_t *win;

static void initialize_spiffs()
{
    esp_vfs_spiffs_conf_t config = {
        .base_path = SPIFFS_PARTITION,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    $(esp_vfs_spiffs_register(&config));

    size_t total_size = 0;
    size_t amount_used = 0;
    $(esp_spiffs_info(config.partition_label, &total_size, &amount_used));
    ESP_LOGD(TAG, "initialize_spiffs total_size=%d, amount_used=%d", total_size, amount_used);
}

static void play_next()
{
    ESP_LOGI(TAG, "play_next progression_index=%d", progression_index);
    if (music && progression_index < ysw_array_get_count(music->progressions)) {
        ysw_progression_t *progression = ysw_array_get(music->progressions, progression_index);
        play_progression(progression);
        progression_index++;
    }
}

static void on_note_on(uint8_t channel, uint8_t midi_note, uint8_t velocity)
{
    ysw_synthesizer_message_t message = {
        .type = YSW_SYNTHESIZER_NOTE_ON,
        .note_on.channel = channel,
        .note_on.midi_note = midi_note,
        .note_on.velocity = velocity,
    };
    ysw_message_send(synthesizer_queue, &message);
}

static void on_note_off(uint8_t channel, uint8_t midi_note)
{
    ysw_synthesizer_message_t message = {
        .type = YSW_SYNTHESIZER_NOTE_OFF,
        .note_off.channel = channel,
        .note_off.midi_note = midi_note,
    };
    ysw_message_send(synthesizer_queue, &message);
}

static void on_program_change(uint8_t channel, uint8_t program)
{
    ysw_synthesizer_message_t message = {
        .type = YSW_SYNTHESIZER_PROGRAM_CHANGE,
        .program_change.channel = channel,
        .program_change.program = program,
    };
    ysw_message_send(synthesizer_queue, &message);
}

static void on_state_change(ysw_sequencer_state_t new_state)
{
    if (new_state == YSW_SEQUENCER_STATE_PLAYBACK_COMPLETE) {
        play_next();
    }
}

static void play_progression(ysw_progression_t *s)
{
    note_t *notes = ysw_progression_get_notes(s);
    assert(notes);

    synthesizer_queue = ysw_ble_synthesizer_create_task();

    ysw_sequencer_config_t config = {
        .on_note_on = on_note_on,
        .on_note_off = on_note_off,
        .on_program_change = on_program_change,
        .on_state_change = on_state_change,
    };

    sequencer_queue = ysw_sequencer_create_task(&config);

    ysw_sequencer_message_t message = {
        .type = YSW_SEQUENCER_INITIALIZE,
        .initialize.notes = notes,
        .initialize.note_count = ysw_progression_get_note_count(s),
    };

    ysw_message_send(sequencer_queue, &message);

    for (int i = 20; i > 0; i--) {
        ESP_LOGW(TAG, "%d - please connect the synthesizer", i);
        wait_millis(1000);
    }

    message = (ysw_sequencer_message_t){
        .type = YSW_SEQUENCER_PLAY,
            .play.loop_count = 1
    };

    ysw_message_send(sequencer_queue, &message);
}

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

    uint32_t chord_count = ysw_music_get_chord_count(music);
    if (chord_index >= chord_count) {
        return;
    }

    ysw_chord_t *chord = ysw_music_get_chord(music, chord_index);

    uint32_t note_count = ysw_chord_get_note_count(chord);
    if (note_index >= note_count) {
        return;
    }

    char buf[MDBUF_SZ];
    snprintf(buf, MDBUF_SZ, "Note (%d of %d)", to_count(note_index), note_count);

    ysw_chord_note_t *chord_note = ysw_chord_get_chord_note(chord, note_index);

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


    create_field(win, "Degree:", ysw_itoa(chord_note->degree, buf, MDBUF_SZ));
    create_field(win, "Start:", ysw_itoa(chord_note->start, buf, MDBUF_SZ));
    create_field(win, "Duration:", ysw_itoa(chord_note->duration, buf, MDBUF_SZ));
    create_field(win, "Volume:", ysw_itoa(chord_note->velocity, buf, MDBUF_SZ));

    lv_win_set_layout(win, LV_LAYOUT_PRETTY);
}

static char *headings[] = {
    "Degree", "Start", "Duration", "Volume"
};

#define COLUMN_COUNT (sizeof(headings) / sizeof(char*))

static lv_obj_t *add_header_button(lv_obj_t *win, const void *img_src, lv_event_cb_t event_cb)
{
    lv_obj_t *btn = lv_win_add_btn(win, img_src);
    if (event_cb) {
        lv_obj_set_event_cb(btn, event_cb);
    }
    return btn;
}

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

static lv_obj_t *add_footer_button(lv_obj_t *footer, const void *img_src, lv_event_cb_t event_cb)
{
    lv_obj_t *editor = lv_obj_get_parent(footer);
    lv_obj_t *win = lv_obj_get_child_back(editor, NULL);
    lv_win_ext_t *ext = lv_obj_get_ext_attr(win);
    lv_obj_t *previous = lv_ll_get_head(&footer->child_ll); // get prev before adding new

    lv_obj_t *btn = lv_btn_create(footer, NULL);

    lv_btn_set_style(btn, LV_BTN_STYLE_REL, ext->style_btn_rel);
    lv_btn_set_style(btn, LV_BTN_STYLE_PR, ext->style_btn_pr);
    lv_obj_set_size(btn, ext->btn_size, ext->btn_size);

    lv_obj_t *img = lv_img_create(btn, NULL);
    lv_obj_set_click(img, false);
    lv_img_set_src(img, img_src);

    if (!previous) {
        lv_obj_align(btn, footer, LV_ALIGN_IN_TOP_LEFT, 0, 0);
    } else {
        lv_obj_align(btn, previous, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    }

    if (event_cb) {
        lv_obj_set_event_cb(btn, event_cb);
    }

    return btn;
}

static void clear_selected_note_highlight()
{
    if (selection.old_note_index != -1) {
        for (int i = 0; i < COLUMN_COUNT; i++) {
            lv_table_set_cell_type(table, selection.old_note_index + 1, i, 1); // +1 for heading
        }
    }
    selection.old_note_index = -1;
    lv_obj_refresh_style(table);
}

static void select_note()
{
    uint32_t chord_count = ysw_music_get_chord_count(music);
    if (chord_index >= chord_count) {
        return;
    }

    ysw_chord_t *chord = ysw_music_get_chord(music, chord_index);

    uint32_t note_count = ysw_chord_get_note_count(chord);
    if (note_index >= note_count) {
        return;
    }

    char buf[MDBUF_SZ];
    snprintf(buf, MDBUF_SZ, "Note %d of %d", to_count(note_index), note_count);
    lv_label_set_text(footer_label, buf);
    lv_obj_realign(footer_label);

    clear_selected_note_highlight();
    for (int i = 0; i < COLUMN_COUNT; i++) {
        lv_table_set_cell_type(table, note_index + 1, i, 4); // +1 for heading
    }
    lv_obj_refresh_style(table);

    selection.old_note_index = note_index;
}

static void select_chord()
{
    // TODO: Preserve (as close as possible) selection across chords
    clear_selected_note_highlight();

    uint32_t chord_count = ysw_music_get_chord_count(music);
    if (chord_index >= chord_count) {
        return;
    }

    ysw_chord_t *chord = ysw_music_get_chord(music, chord_index);

    lv_win_set_title(win, chord->name);

    uint32_t note_count = ysw_chord_get_note_count(chord);
    lv_table_set_row_cnt(table, note_count + 1); // +1 for the headings

    for (uint32_t i = 0; i < note_count; i++) {
        ESP_LOGD(TAG, "setting note attributes, i=%d", i);
        char buffer[16];
        int n = i + 1;
        ysw_chord_note_t *chord_note = ysw_chord_get_chord_note(chord, i);
        lv_table_set_cell_value(table, n, 0, ysw_itoa(chord_note->degree, buffer, sizeof(buffer)));
        lv_table_set_cell_value(table, n, 1, ysw_itoa(chord_note->start, buffer, sizeof(buffer)));
        lv_table_set_cell_value(table, n, 2, ysw_itoa(chord_note->duration, buffer, sizeof(buffer)));
        lv_table_set_cell_value(table, n, 3, ysw_itoa(chord_note->velocity, buffer, sizeof(buffer)));
        for (int j = 0; j < COLUMN_COUNT; j++) {
            lv_table_set_cell_align(table, n, j, LV_LABEL_ALIGN_CENTER);
        }
    }

    if (note_count) {
        note_index = 0;
        select_note();
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
            note_index = row - 1; // -1 for headings
            select_note();
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

void select_next_chord(lv_obj_t * btn, lv_event_t event)
{
    if(event == LV_EVENT_RELEASED) {
        ESP_LOGD(TAG, "next old chord_index=%d", chord_index);
        uint32_t chord_count = ysw_music_get_chord_count(music);
        if (++chord_index >= chord_count) {
            ESP_LOGD(TAG, "wrapping chord_index=%d", chord_index);
            chord_index = 0;
        }
        ESP_LOGD(TAG, "new chord_index=%d", chord_index);
        select_chord(chord_index);
    }
}

void select_prev_chord(lv_obj_t * btn, lv_event_t event)
{
    if(event == LV_EVENT_RELEASED) {
        ESP_LOGD(TAG, "prev old chord_index=%d", chord_index);
        uint32_t chord_count = ysw_music_get_chord_count(music);
        if (--chord_index >= chord_count) {
            ESP_LOGD(TAG, "wrapping chord_index=%d", chord_index);
            chord_index = chord_count - 1;
        }
        select_chord(chord_index);
        ESP_LOGD(TAG, "new chord_index=%d", chord_index);
    }
}

static void display_chords()
{
    lv_coord_t display_w = lv_disp_get_hor_res(NULL);
    lv_coord_t display_h = lv_disp_get_ver_res(NULL);
    lv_coord_t footer_h = BUTTON_SIZE + 5 + 5;

    lv_obj_t *editor = lv_obj_create(lv_scr_act(), NULL);
    lv_obj_set_style(editor, &plain_color_tight);
    lv_obj_set_size(editor, display_w, display_h);

    win = lv_win_create(editor, NULL);
    lv_win_set_style(win, LV_WIN_STYLE_BG, &lv_style_pretty);
    lv_win_set_style(win, LV_WIN_STYLE_CONTENT, &win_style_content);
    lv_win_set_title(win, "Chord Name");
    lv_win_set_btn_size(win, BUTTON_SIZE);
    lv_obj_set_height(win, display_h - footer_h);

    lv_obj_t *footer = lv_obj_create(editor, NULL);
    lv_obj_set_size(footer, display_w, footer_h);
    lv_obj_align(footer, win, LV_ALIGN_OUT_BOTTOM_RIGHT, 5, 5);

    add_footer_button(footer, LV_SYMBOL_SETTINGS, NULL);
    add_footer_button(footer, LV_SYMBOL_PLUS, NULL);
    add_footer_button(footer, LV_SYMBOL_MINUS, NULL);
    add_footer_button(footer, LV_SYMBOL_UP, NULL);
    add_footer_button(footer, LV_SYMBOL_DOWN, NULL);
    add_footer_button(footer, LV_SYMBOL_LEFT, NULL);
    add_footer_button(footer, LV_SYMBOL_RIGHT, NULL);

    footer_label = lv_label_create(footer, NULL);
    lv_label_set_text(footer_label, "Chord Note");
    lv_obj_align(footer_label, footer, LV_ALIGN_IN_TOP_RIGHT, -20, 0);

    add_header_button(win, LV_SYMBOL_CLOSE, lv_win_close_event_cb);
    add_header_button(win, LV_SYMBOL_NEXT, select_next_chord);
    add_header_button(win, LV_SYMBOL_LOOP, NULL);
    add_header_button(win, LV_SYMBOL_PAUSE, NULL);
    add_header_button(win, LV_SYMBOL_PLAY, NULL);
    add_header_button(win, LV_SYMBOL_PREV, select_prev_chord);

    lv_obj_t *page = lv_win_get_content(win);

    // Trying to get rid of one pixel to left of table. It's not these:
    //lv_page_set_style(page, LV_PAGE_STYLE_BG, &page_bg_style);
    //lv_win_set_style(win, LV_WIN_STYLE_BG, &page_bg_style);

    lv_page_set_style(page, LV_PAGE_STYLE_SCRL, &page_scrl_style);

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

    chord_index = 0;
    select_chord();
}

static void process_chords()
{
    if (ysw_music_get_chord_count(music)) {
        display_chords();
    } else {
        // TODO: start chord editor
    }
}

void mbox_callback(struct _lv_obj_t * obj, lv_event_t event)
{
}

void app_main()
{
    esp_log_level_set("BLEServer", ESP_LOG_INFO);
    esp_log_level_set("BLEDevice", ESP_LOG_INFO);
    esp_log_level_set("BLECharacteristic", ESP_LOG_INFO);

    esp_log_level_set("YSW_HEAP", ESP_LOG_INFO);
    esp_log_level_set("YSW_ARRAY", ESP_LOG_INFO);

    eli_ili9341_xpt2046_config_t new_config = {
        .mosi = 21,
        .clk = 22,
        .ili9341_cs = 5,
        .xpt2046_cs = 32,
        .dc = 19,
        .rst = 18,
        .bckl = 23,
        .miso = 27,
        .irq = 14,
        .x_min = 346,
        .y_min = 235,
        .x_max = 3919,
        .y_max = 3883,
        .spi_host = HSPI_HOST,
    };

    eli_ili9341_xpt2046_initialize(&new_config);

    initialize_spiffs();
    ysw_lv_styles_initialize();

    music = ysw_music_parse(MUSIC_DEFINITIONS);

    if (music) {
        process_chords();
    } else {
        lv_obj_t * mbox1 = lv_mbox_create(lv_scr_act(), NULL);
        lv_mbox_set_text(mbox1, "The music partition is empty");
        lv_obj_set_width(mbox1, 200);
        lv_obj_set_event_cb(mbox1, mbox_callback);
        lv_obj_align(mbox1, NULL, LV_ALIGN_CENTER, 0, 0);
    }

    // play_next();

    while (1) {
        wait_millis(1);
        lv_task_handler();
    }
}
