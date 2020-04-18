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

#include "ysw_synthesizer.h"
#include "ysw_ble_synthesizer.h"
#include "ysw_sequencer.h"
#include "ysw_message.h"
#include "ysw_music.h"
#include "ysw_music_parser.h"

#include "driver/spi_master.h"
#include "lvgl/lvgl.h"
#include "eli_ili9341_xpt2046.h"

#define TAG "MAIN"

typedef struct {
    int16_t row;
    int16_t column;
} selection_t;

static selection_t selection = {
    .row = -1,
};

static lv_obj_t *kb;
static lv_obj_t *page;
static lv_obj_t *table;
static lv_signal_cb_t old_signal_cb;
static lv_style_t value_cell;
static lv_style_t win_style_content;

static QueueHandle_t synthesizer_queue;
static QueueHandle_t sequencer_queue;

static ysw_music_t *music;
static uint32_t progression_index;

static void play_progression(ysw_progression_t *s);

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
    ysw_progression_set_percent_tempo(s, 100);
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
        .initialize.note_count = ysw_get_note_count(s),
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

static void keyboard_event_cb(lv_obj_t * keyboard, lv_event_t event)
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

static void open_value_editor(int16_t row, int16_t column)
{
    lv_obj_t * win = lv_win_create(lv_scr_act(), NULL);
    lv_obj_align(win, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_win_set_style(win, LV_WIN_STYLE_BG, &lv_style_pretty);
    lv_win_set_style(win, LV_WIN_STYLE_CONTENT, &win_style_content);
    lv_win_set_title(win, "Chord Note (1 of 8)");
    lv_win_set_layout(win, LV_LAYOUT_OFF);

    lv_obj_t * close_btn = lv_win_add_btn(win, LV_SYMBOL_CLOSE);
    lv_obj_set_event_cb(close_btn, lv_win_close_event_cb);

    lv_win_add_btn(win, LV_SYMBOL_SETTINGS);
    lv_win_set_btn_size(win, 20);

    //lv_win_ext_t *ext = lv_obj_get_ext_attr(win);
    //lv_obj_t *scrl = lv_page_get_scrl(ext->page);

    create_field(win, "Degree:", "1");
    create_field(win, "Volume:", "100");
    create_field(win, "Time:", "0");
    create_field(win, "Duration:", "230");

    lv_win_set_layout(win, LV_LAYOUT_PRETTY);
}

static lv_res_t my_scrl_signal_cb(lv_obj_t *scrl, lv_signal_t sign, void *param)
{
    //ESP_LOGD(TAG, "my_scrl_signal_cb sign=%d", sign);
    if (sign == LV_SIGNAL_PRESSED) {
        lv_indev_t* indev_act = (lv_indev_t*)param;
        lv_indev_proc_t *proc = &indev_act->proc;
        lv_area_t scrl_coords;
        lv_obj_get_coords(scrl, &scrl_coords);
        int16_t row = (proc->types.pointer.act_point.y + -scrl_coords.y1) / 30;
        int16_t column = proc->types.pointer.act_point.x / 79;
        ESP_LOGD(TAG, "act_point x=%d, y=%d, scrl_coords x1=%d, y1=%d, x2=%d, y2=%d, row+1=%d, column+1=%d", proc->types.pointer.act_point.x, proc->types.pointer.act_point.y, scrl_coords.x1, scrl_coords.y1, scrl_coords.x2, scrl_coords.y2, row+1, column+1);

        // TODO: use a metdata structure to hold info about the cells
        if (row == 0 || row == 3) {
        } else if (row < 3 && column == 0) {
        } else {
            if (selection.row != -1) {
                lv_table_set_cell_type(table, selection.row, selection.column, 1);
            }
            lv_table_set_cell_type(table, row, column, 4);
            lv_obj_refresh_style(table);
            if (selection.row == row && selection.column == column) {
                open_value_editor(row, column);
            } else {
                selection.row = row;
                selection.column = column;
            }
        }
    }
    return old_signal_cb(scrl, sign, param);
}

    UNUSED
static void create_marquis_mock_up()
{
    static lv_style_t page_bg_style;
    lv_style_copy(&page_bg_style, &lv_style_pretty_color);
    ESP_LOGD(TAG, "page_bg_style radius=%d, width=%d, part=%#x, padding top=%d, bottom=%d, left=%d, right=%d, inner=%d", page_bg_style.body.radius, page_bg_style.body.border.width, page_bg_style.body.border.part, page_bg_style.body.padding.top, page_bg_style.body.padding.bottom, page_bg_style.body.padding.left, page_bg_style.body.padding.right, page_bg_style.body.padding.inner);
    page_bg_style.body.radius = 0;
    page_bg_style.body.border.width = 0;
    page_bg_style.body.border.part = LV_BORDER_NONE;
    page_bg_style.body.padding.top = 0;
    page_bg_style.body.padding.bottom = 0;
    page_bg_style.body.padding.left = 0;
    page_bg_style.body.padding.right = 0;
    page_bg_style.body.padding.inner = 0;

    static lv_style_t page_scrl_style;
    lv_style_copy(&page_scrl_style, &lv_style_pretty_color);
    ESP_LOGD(TAG, "page_scrl_style radius=%d, width=%d, part=%#x, padding top=%d, bottom=%d, left=%d, right=%d, inner=%d", page_scrl_style.body.radius, page_scrl_style.body.border.width, page_scrl_style.body.border.part, page_scrl_style.body.padding.top, page_scrl_style.body.padding.bottom, page_scrl_style.body.padding.left, page_scrl_style.body.padding.right, page_scrl_style.body.padding.inner);
    page_scrl_style.body.radius = 0;
    page_scrl_style.body.border.width = 0;
    page_scrl_style.body.border.part = LV_BORDER_NONE;
    page_scrl_style.body.padding.top = 0;
    page_scrl_style.body.padding.bottom = 0;
    page_scrl_style.body.padding.left = 0;
    page_scrl_style.body.padding.right = 0;
    page_scrl_style.body.padding.inner = 0;

    static lv_style_t table_bg_style;
    lv_style_copy(&table_bg_style, &lv_style_plain_color);
    ESP_LOGD(TAG, "table_bg_style radius=%d, width=%d, part=%#x, padding top=%d, bottom=%d, left=%d, right=%d, inner=%d", table_bg_style.body.radius, table_bg_style.body.border.width, table_bg_style.body.border.part, table_bg_style.body.padding.top, table_bg_style.body.padding.bottom, table_bg_style.body.padding.left, table_bg_style.body.padding.right, table_bg_style.body.padding.inner);
    table_bg_style.body.radius = 0;
    table_bg_style.body.border.width = 0;
    table_bg_style.body.border.part = LV_BORDER_NONE;
    table_bg_style.body.padding.top = 0;
    table_bg_style.body.padding.bottom = 0;
    table_bg_style.body.padding.left = 0;
    table_bg_style.body.padding.right = 0;
    table_bg_style.body.padding.inner = 0;

    lv_style_copy(&win_style_content, &lv_style_transp);
    ESP_LOGD(TAG, "win_style_content radius=%d, width=%d, part=%#x, padding top=%d, bottom=%d, left=%d, right=%d, inner=%d", win_style_content.body.radius, win_style_content.body.border.width, win_style_content.body.border.part, win_style_content.body.padding.top, win_style_content.body.padding.bottom, win_style_content.body.padding.left, win_style_content.body.padding.right, win_style_content.body.padding.inner);
    win_style_content.body.radius = 0;
    win_style_content.body.border.width = 0;
    win_style_content.body.border.part = LV_BORDER_NONE;
    win_style_content.body.padding.top = 5;
    win_style_content.body.padding.bottom = 0;
    win_style_content.body.padding.left = 5;
    win_style_content.body.padding.right = 0;
    win_style_content.body.padding.inner = 5;

    /*Create a normal cell style*/
    lv_style_copy(&value_cell, &lv_style_plain);
    value_cell.body.border.width = 1;
    value_cell.body.border.color = LV_COLOR_BLACK;

    /*Create a title cell style*/
    static lv_style_t title_cell;
    lv_style_copy(&title_cell, &lv_style_plain);
    title_cell.body.border.width = 1;
    title_cell.body.border.color = LV_COLOR_BLACK;
    title_cell.body.main_color = LV_COLOR_RED;
    title_cell.body.grad_color = LV_COLOR_RED;
    title_cell.text.color = LV_COLOR_BLACK;

    /*Create a field name cell style*/
    static lv_style_t name_cell;
    lv_style_copy(&name_cell, &lv_style_plain);
    name_cell.body.border.width = 1;
    name_cell.body.border.color = LV_COLOR_BLACK;
    name_cell.body.main_color = LV_COLOR_SILVER;
    name_cell.body.grad_color = LV_COLOR_SILVER;

    /*Create a selection cell style*/
    static lv_style_t selected_cell;
    lv_style_copy(&selected_cell, &lv_style_plain);
    selected_cell.body.border.width = 1;
    selected_cell.body.border.color = LV_COLOR_BLACK;
    selected_cell.body.main_color = LV_COLOR_YELLOW;
    selected_cell.body.grad_color = LV_COLOR_YELLOW;

    page = lv_page_create(lv_scr_act(), NULL);
    lv_obj_set_size(page, 320, 240);
    lv_obj_align(page, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_page_set_style(page, LV_PAGE_STYLE_BG, &page_bg_style);
    lv_page_set_style(page, LV_PAGE_STYLE_SCRL, &page_scrl_style);


    table = lv_table_create(page, NULL);
    lv_table_set_style(table, LV_TABLE_STYLE_BG, &table_bg_style);
    lv_table_set_style(table, LV_TABLE_STYLE_CELL1, &value_cell);
    lv_table_set_style(table, LV_TABLE_STYLE_CELL2, &title_cell);
    lv_table_set_style(table, LV_TABLE_STYLE_CELL3, &name_cell);
    lv_table_set_style(table, LV_TABLE_STYLE_CELL4, &selected_cell);

    lv_table_set_col_cnt(table, 4);
    lv_table_set_row_cnt(table, 12);
    lv_obj_align(table, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

    lv_table_set_cell_type(table, 0, 0, 2);
    lv_table_set_cell_align(table, 0, 0, LV_LABEL_ALIGN_CENTER);

    lv_table_set_cell_type(table, 1, 0, 3);
    lv_table_set_cell_align(table, 1, 0, LV_LABEL_ALIGN_RIGHT);

    lv_table_set_cell_type(table, 2, 0, 3);
    lv_table_set_cell_align(table, 2, 0, LV_LABEL_ALIGN_RIGHT);

    lv_table_set_cell_type(table, 3, 0, 3);
    lv_table_set_cell_align(table, 3, 0, LV_LABEL_ALIGN_CENTER);

    lv_table_set_cell_type(table, 3, 1, 3);
    lv_table_set_cell_align(table, 3, 1, LV_LABEL_ALIGN_CENTER);

    lv_table_set_cell_type(table, 3, 2, 3);
    lv_table_set_cell_align(table, 3, 2, LV_LABEL_ALIGN_CENTER);

    lv_table_set_cell_type(table, 3, 3, 3);
    lv_table_set_cell_align(table, 3, 3, LV_LABEL_ALIGN_CENTER);

    lv_obj_t *scroller = lv_page_get_scrl(page);
    old_signal_cb = lv_obj_get_signal_cb(scroller);
    lv_obj_set_signal_cb(scroller, my_scrl_signal_cb);

    lv_table_set_col_width(table, 0, 79);
    lv_table_set_col_width(table, 1, 79);
    lv_table_set_col_width(table, 2, 79);
    lv_table_set_col_width(table, 3, 79);

    lv_table_set_cell_merge_right(table, 0, 2, true);
    lv_table_set_cell_merge_right(table, 0, 1, true);
    lv_table_set_cell_merge_right(table, 0, 0, true);

    lv_table_set_cell_merge_right(table, 1, 2, true);
    lv_table_set_cell_merge_right(table, 1, 1, true);

    lv_table_set_cell_merge_right(table, 2, 2, true);
    lv_table_set_cell_merge_right(table, 2, 1, true);

    lv_table_set_cell_value(table, 0, 0, "Chord Style");

    lv_table_set_cell_value(table, 1, 0, "Name");
    lv_table_set_cell_value(table, 1, 1, "Up / Down");
    lv_table_set_cell_value(table, 2, 0, "Duration");
    lv_table_set_cell_value(table, 2, 1, "2000 (minimum 1980)");

    lv_table_set_cell_value(table, 3, 0, "Degree");
    lv_table_set_cell_value(table, 3, 1, "Volume");
    lv_table_set_cell_value(table, 3, 2, "Time");
    lv_table_set_cell_value(table, 3, 3, "Duration");

    lv_table_set_cell_value(table, 4, 0, "1");
    lv_table_set_cell_value(table, 4, 1, "100");
    lv_table_set_cell_value(table, 4, 2, "0");
    lv_table_set_cell_value(table, 4, 3, "230");

    lv_table_set_cell_value(table, 5, 0, "3");
    lv_table_set_cell_value(table, 5, 1, "80");
    lv_table_set_cell_value(table, 5, 2, "250");
    lv_table_set_cell_value(table, 5, 3, "230");

    lv_table_set_cell_value(table, 6, 0, "5");
    lv_table_set_cell_value(table, 6, 1, "80");
    lv_table_set_cell_value(table, 6, 2, "500");
    lv_table_set_cell_value(table, 6, 3, "230");

    lv_table_set_cell_value(table, 7, 0, "6");
    lv_table_set_cell_value(table, 7, 1, "80");
    lv_table_set_cell_value(table, 7, 2, "750");
    lv_table_set_cell_value(table, 7, 3, "230");

    lv_table_set_cell_value(table, 8, 0, "7");
    lv_table_set_cell_value(table, 8, 1, "100");
    lv_table_set_cell_value(table, 8, 2, "1000");
    lv_table_set_cell_value(table, 8, 3, "230");

    lv_table_set_cell_value(table, 9, 0, "6");
    lv_table_set_cell_value(table, 9, 1, "80");
    lv_table_set_cell_value(table, 9, 2, "1250");
    lv_table_set_cell_value(table, 9, 3, "230");

    lv_table_set_cell_value(table, 10, 0, "5");
    lv_table_set_cell_value(table, 10, 1, "80");
    lv_table_set_cell_value(table, 10, 2, "1500");
    lv_table_set_cell_value(table, 10, 3, "230");

    lv_table_set_cell_value(table, 11, 0, "3");
    lv_table_set_cell_value(table, 11, 1, "80");
    lv_table_set_cell_value(table, 11, 2, "1750");
    lv_table_set_cell_value(table, 11, 3, "230");
}

#define SPIFFS_PARTITION "/spiffs"
#define MUSIC_DEFINITIONS "/spiffs/music.csv"

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

    create_marquis_mock_up();

    music = ysw_music_parse(MUSIC_DEFINITIONS);

    uint32_t chord_count = ysw_array_get_count(music->chords);
    for (uint32_t i = 0; i < chord_count; i++) {
        ysw_chord_t *chord = ysw_array_get(music->chords, i);
        ESP_LOGI(TAG, "chord[%d]=%s", i, chord->name);
        ysw_chord_dump(TAG, chord);
    }

    uint32_t progression_count = ysw_array_get_count(music->progressions);
    for (uint32_t i = 0; i < progression_count; i++) {
        ysw_progression_t *progression = ysw_array_get(music->progressions, i);
        ESP_LOGI(TAG, "progression[%d]=%s", i, progression->name);
        ysw_progression_dump(TAG, progression);
    }

    play_next();

    while (1) {
        wait_millis(1);
        lv_task_handler();
    }
}
