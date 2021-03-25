// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_performer.h"
#include "ysw_app.h"
#include "ysw_common.h"
#include "ysw_event.h"
#include "ysw_footer.h"
#include "ysw_header.h"
#include "ysw_heap.h"
#include "ysw_menu.h"
#include "ysw_midi.h"
#include "ysw_popup.h"
#include "ysw_staff.h"
#include "ysw_string.h"
#include "ysw_style.h"
#include "ysw_task.h"
#include "ysw_ticks.h"
#include "zm_music.h"
#include "lvgl.h"
#include "esp_log.h"
#include "assert.h"
#include "limits.h"
#include "stdlib.h"

#define TAG "YSW_PERFORMER"

#define DEFAULT_CHORD_TYPE 0
#define DEFAULT_CHORD_STYLE 0
#define DEFAULT_BEAT 0

// channels

#define BASE_CHANNEL 0
#define MELODY_CHANNEL (BASE_CHANNEL+0)
#define CHORD_CHANNEL (BASE_CHANNEL+1)
#define RHYTHM_CHANNEL (BASE_CHANNEL+2)

#define BACKGROUND_BASE (BASE_CHANNEL+3)
#define BACKGROUND_MELODY (BACKGROUND_BASE+0)
#define BACKGROUND_CHORD (BACKGROUND_BASE+1)
#define BACKGROUND_RHYTHM (BACKGROUND_BASE+2)

typedef enum {
    YSW_PERFORMER_MODE_MELODY,
    YSW_PERFORMER_MODE_CHORD,
    YSW_PERFORMER_MODE_RHYTHM,
    YSW_PERFORMER_MODE_HARP,
} ysw_performer_mode_t;

typedef struct {
    ysw_bus_t *bus;
    ysw_menu_t *menu;
    QueueHandle_t queue;

    zm_music_t *music;
    zm_program_x melody_program;
    zm_program_x chord_program;

    zm_chord_type_t *chord_type;
    zm_note_t root;

    zm_time_x down_at;
    zm_time_x up_at;
    zm_time_x delta;

    ysw_performer_mode_t mode;

    lv_obj_t *container;

    ysw_performer_mode_t program_builder;

} ysw_performer_t;

static void fire_bank_select(ysw_performer_t *performer, zm_channel_x channel, zm_bank_x bank)
{
    ysw_event_bank_select_t bank_select = {
        .channel = channel,
        .bank = bank,
    };
    ysw_event_fire_bank_select(performer->bus, YSW_ORIGIN_EDITOR, &bank_select);
}

static void fire_program_change(ysw_performer_t *performer, zm_channel_x channel, zm_program_x program)
{
    ysw_event_program_change_t program_change = {
        .channel = channel,
        .program = program,
    };
    ysw_event_fire_program_change(performer->bus, YSW_ORIGIN_EDITOR, &program_change);
}

static void fire_note_on(ysw_performer_t *performer, zm_channel_x channel, zm_note_t midi_note)
{
    ysw_event_note_on_t note_on = {
        .channel = channel,
        .midi_note = midi_note,
        .velocity = 80,
    };
    ysw_event_fire_note_on(performer->bus, YSW_ORIGIN_EDITOR, &note_on);
}

static void fire_note_off(ysw_performer_t *performer, zm_channel_x channel, zm_note_t midi_note)
{
    ysw_event_note_off_t note_off = {
        .channel = channel,
        .midi_note = midi_note,
    };
    ysw_event_fire_note_off(performer->bus, YSW_ORIGIN_EDITOR, &note_off);
}

// Setters

static void set_program(ysw_performer_t *performer, ysw_performer_mode_t mode, zm_program_x program)
{
    switch (mode) {
        case YSW_PERFORMER_MODE_MELODY:
        case YSW_PERFORMER_MODE_HARP:
            performer->melody_program = program;
            fire_program_change(performer, MELODY_CHANNEL, performer->melody_program);
            break;
        case YSW_PERFORMER_MODE_CHORD:
            performer->chord_program = program;
            fire_program_change(performer, CHORD_CHANNEL, performer->chord_program);
            break;
        case YSW_PERFORMER_MODE_RHYTHM:
            // no program change for rhythm channel
            break;
    }
}

static void set_chord_type(ysw_performer_t *performer, zm_chord_type_x chord_type_x)
{
    performer->chord_type = ysw_array_get(performer->music->chord_types, chord_type_x);
}

// Steps

static void press_note(ysw_performer_t *performer, zm_note_t midi_note, uint32_t down_at)
{
    performer->down_at = down_at;
    performer->delta = performer->down_at - performer->up_at;

    switch (performer->mode) {
        case YSW_PERFORMER_MODE_MELODY:
        case YSW_PERFORMER_MODE_HARP:
            fire_note_on(performer, MELODY_CHANNEL, midi_note);
            break;
        case YSW_PERFORMER_MODE_RHYTHM:
            fire_note_on(performer, RHYTHM_CHANNEL, midi_note);
            break;
        case YSW_PERFORMER_MODE_CHORD:
            break;
    }
}

static void release_note(ysw_performer_t *performer, zm_note_t midi_note, uint32_t up_at, uint32_t duration)
{
    performer->up_at = up_at;
    switch (performer->mode) {
        case YSW_PERFORMER_MODE_MELODY:
        case YSW_PERFORMER_MODE_HARP:
            fire_note_off(performer, MELODY_CHANNEL, midi_note);
            break;
        case YSW_PERFORMER_MODE_RHYTHM:
            fire_note_off(performer, RHYTHM_CHANNEL, midi_note);
            break;
        case YSW_PERFORMER_MODE_CHORD:
            break;
    }
}

// Generators

#define YSW_APP_SOFTKEY_SZ 16

// TODO: use ysw_app_softkey_map_t

static const uint8_t softkey_map[] = {
    YSW_R1_C1,
    YSW_R1_C2,
    YSW_R1_C3,
    YSW_R1_C4,

    YSW_R2_C1,
    YSW_R2_C2,
    YSW_R2_C3,
    YSW_R2_C4,

    YSW_R3_C1,
    YSW_R3_C2,
    YSW_R3_C3,
    YSW_R3_C4,

    YSW_R4_C1,
    YSW_R4_C2,
    YSW_R4_C3,
    YSW_R4_C4,
};

static void initialize_item(ysw_menu_item_t *item, uint32_t softkey, const char *name, ysw_menu_cb_t cb, intptr_t value, const ysw_menu_item_t *submenu)
{
    item->softkey = softkey;
    item->name = name;
    item->flags = submenu ? YSW_MF_REPLACE : YSW_MF_PRESS;
    item->cb = cb;
    item->value = value;
    item->submenu = submenu;
}

static void finalize_menu(ysw_menu_item_t *p, const char *name)
{
    p->softkey = YSW_R4_C1;
    p->name = "Back";
    p->flags = YSW_MF_MINUS;
    p->cb = ysw_menu_nop;
    p->value = 0;
    p->submenu = NULL;
    p++;

    p->name = name;
    p->flags = YSW_MF_END;
}

static const ysw_menu_item_t root_menu[];
static const ysw_menu_item_t program_category_1_menu[];
static void on_program_1(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item);

static void generate_chord_type_menu(ysw_performer_t *performer, ysw_menu_cb_t cb, ysw_menu_item_t *template, const ysw_menu_item_t *submenu, const char *name)
{
    zm_chord_type_x count = ysw_array_get_count(performer->music->chord_types);
    count = min(count, 12);
    ysw_menu_item_t *p = template;
    for (zm_chord_type_x i = 0; i < count; i++) {
        zm_chord_type_t *chord_type = ysw_array_get(performer->music->chord_types, i);
        initialize_item(p, softkey_map[i], chord_type->label, cb, i, submenu);
        p++;
    }

    initialize_item(p++, YSW_BUTTON_2, "Root", ysw_menu_nop, 0, root_menu);
    initialize_item(p++, YSW_BUTTON_3, "Program", on_program_1, 0, program_category_1_menu);
    finalize_menu(p, name);
}

static ysw_menu_item_t chord_type_template[YSW_APP_SOFTKEY_SZ + 3 + 1];
static void on_settings_chord_type_1(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item);

static void generate_program_menu(ysw_performer_t *performer, ysw_menu_cb_t cb, ysw_menu_item_t *template, uint8_t category, const char *name)
{
    ysw_menu_item_t *p = template;
    for (zm_chord_style_x i = 0; i < 8; i++) {
        uint8_t program = (category * 8) + i;
        const char *label = ysw_midi_get_program_label(program);
        initialize_item(p, softkey_map[p - template], label, cb, program, NULL);
        p++;
    }
    initialize_item(p++, YSW_BUTTON_1, "Type", on_settings_chord_type_1, 0, chord_type_template);
    initialize_item(p++, YSW_BUTTON_2, "Root", ysw_menu_nop, 0, root_menu);
    finalize_menu(p, name);
}

// Handlers

static void on_mode_melody(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_performer_t *performer = menu->context;
    performer->mode = YSW_PERFORMER_MODE_MELODY;
}

static void on_mode_chord(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_performer_t *performer = menu->context;
    performer->mode = YSW_PERFORMER_MODE_CHORD;
}

static void on_mode_rhythm(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_performer_t *performer = menu->context;
    performer->mode = YSW_PERFORMER_MODE_RHYTHM;
}

static void on_mode_harp(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_performer_t *performer = menu->context;
    performer->mode = YSW_PERFORMER_MODE_HARP;
}

static ysw_menu_item_t program_template[YSW_APP_SOFTKEY_SZ + 1];

static void on_program_3(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_performer_t *performer = menu->context;
    set_program(performer, performer->program_builder, item->value);
}

static void on_program_2(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_performer_t *performer = menu->context;
    generate_program_menu(performer, on_program_3, program_template, item->value, "Program");
}

static void on_program_1(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_performer_t *performer = menu->context;
    performer->program_builder = item->value;
}

static void on_settings_chord_type_2(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_performer_t *performer = menu->context;
    set_chord_type(performer, item->value);
}

static void on_settings_chord_type_1(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_performer_t *performer = menu->context;
    generate_chord_type_menu(performer, on_settings_chord_type_2, chord_type_template, NULL, "Type");
}

static void on_root(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_performer_t *performer = menu->context;
    performer->root = item->value;
}

static void on_headphone_volume(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_performer_t *performer = menu->context;
    ysw_event_fire_amp_volume(performer->bus, item->value);
#ifdef IDF_VER
    // TODO: move this to a codec task
    ESP_LOGD(TAG, "headphone_volume=%d", item->value);
    extern esp_err_t ac101_set_earph_volume(int volume);
	$(ac101_set_earph_volume(((item->value % 101) * 63) / 100));
#endif
}

static void on_speaker_volume(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_performer_t *performer = menu->context;
    ysw_event_fire_amp_volume(performer->bus, item->value);
#ifdef IDF_VER
    // TODO: move this to a codec task
    ESP_LOGD(TAG, "speaker_volume=%d", item->value);
    extern esp_err_t ac101_set_spk_volume(int volume);
	$(ac101_set_spk_volume(((item->value % 101) * 63) / 100));
#endif
}
static void on_synth_gain(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_performer_t *performer = menu->context;
    ysw_event_fire_synth_gain(performer->bus, item->value);
}

static void close_performer(ysw_performer_t *performer)
{
    ysw_event_fire_stop(performer->bus);
    ysw_app_terminate();
}

static void on_close(ysw_menu_t *menu, ysw_event_t *event, ysw_menu_item_t *item)
{
    ysw_performer_t *performer = menu->context;
    close_performer(performer);
}

static int8_t notekey_types[] = {
    +1, -1, +2, -2, +3, +4, -3, +5, -4, +6, -5, +7, +8, -6, +9, -7, +10, +11, -8, +12, -9, +13, -10, +14, +15,
};

#define NOTE_INDEX_SZ (sizeof(notekey_types) / sizeof(notekey_types[0]))

static void on_notekey_down(ysw_performer_t *performer, ysw_event_notekey_down_t *m)
{
    if (performer->mode == YSW_PERFORMER_MODE_HARP) {
        int8_t semitone = (m->midi_note - 60) % NOTE_INDEX_SZ;
        int8_t notekey_type = notekey_types[semitone];
        if (notekey_type > 0) {
            uint32_t distance_count = ysw_array_get_count(performer->chord_type->distances);
            int8_t note_index = notekey_type - 1;
            int8_t distance_index = note_index % distance_count;
            int8_t octave = note_index / distance_count;
            int8_t distance = YSW_INT ysw_array_get(performer->chord_type->distances, distance_index);
            zm_note_t note = 36 + performer->root + (octave * 12) + distance;
            // ESP_LOGD(TAG, "input=%d, note_index=%d, distance_index=%d, octave=%d, distance=%d, output=%d", m->midi_note, note_index, distance_index, octave, distance, note);
            press_note(performer, note, m->time);
        }
    } else {
        press_note(performer, m->midi_note, m->time);
    }
}

static void on_notekey_up(ysw_performer_t *performer, ysw_event_notekey_up_t *m)
{
    if (performer->mode == YSW_PERFORMER_MODE_HARP) {
    } else {
        release_note(performer, m->midi_note, m->time, m->duration);
    }
}

static void process_event(void *context, ysw_event_t *event)
{
    ysw_performer_t *performer = context;
    switch (event->header.type) {
        case YSW_EVENT_NOTEKEY_DOWN:
            on_notekey_down(performer, &event->notekey_down);
            break;
        case YSW_EVENT_NOTEKEY_UP:
            on_notekey_up(performer, &event->notekey_up);
            break;
        case YSW_EVENT_SOFTKEY_DOWN:
            ysw_menu_on_softkey_down(performer->menu, event);
            break;
        case YSW_EVENT_SOFTKEY_UP:
            ysw_menu_on_softkey_up(performer->menu, event);
            break;
        case YSW_EVENT_SOFTKEY_PRESSED:
            ysw_menu_on_softkey_pressed(performer->menu, event);
            break;
        default:
            break;
    }
}

#if 0
static const ysw_menu_item_t menu_template[] = {
    { YSW_R1_C1, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C2, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C3, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R1_C4, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R2_C1, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R2_C2, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R2_C3, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R2_C4, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R3_C1, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R3_C2, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R3_C3, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R3_C4, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C2, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R4_C3, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },
    { YSW_R4_C4, " ", YSW_MF_NOP, ysw_menu_nop, 0, NULL },

    { 0, "Menu Name", YSW_MF_END, NULL, 0, NULL },
};
#endif

static const ysw_menu_item_t headphone_volume_menu[] = {
    { YSW_R1_C1, "10\n(Soft)", YSW_MF_COMMAND, on_headphone_volume, 10, NULL },
    { YSW_R1_C2, "20", YSW_MF_COMMAND, on_headphone_volume, 25, NULL },
    { YSW_R1_C3, "30", YSW_MF_COMMAND, on_headphone_volume, 50, NULL },

    { YSW_R2_C1, "40", YSW_MF_COMMAND, on_headphone_volume, 75, NULL },
    { YSW_R2_C2, "50\n(Normal)", YSW_MF_COMMAND, on_headphone_volume, 100, NULL },
    { YSW_R2_C3, "60", YSW_MF_COMMAND, on_headphone_volume, 200, NULL },

    { YSW_R3_C1, "70", YSW_MF_COMMAND, on_headphone_volume, 300, NULL },
    { YSW_R3_C2, "80", YSW_MF_COMMAND, on_headphone_volume, 400, NULL },
    { YSW_R3_C3, "90\n(Loud)", YSW_MF_COMMAND, on_headphone_volume, 500, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C4, "Off", YSW_MF_COMMAND, on_headphone_volume, 0, NULL },

    { 0, "Headphone Volume", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t speaker_volume_menu[] = {
    { YSW_R1_C1, "10\n(Soft)", YSW_MF_COMMAND, on_speaker_volume, 10, NULL },
    { YSW_R1_C2, "20", YSW_MF_COMMAND, on_speaker_volume, 25, NULL },
    { YSW_R1_C3, "30", YSW_MF_COMMAND, on_speaker_volume, 50, NULL },

    { YSW_R2_C1, "40", YSW_MF_COMMAND, on_speaker_volume, 75, NULL },
    { YSW_R2_C2, "50\n(Normal)", YSW_MF_COMMAND, on_speaker_volume, 100, NULL },
    { YSW_R2_C3, "60", YSW_MF_COMMAND, on_speaker_volume, 200, NULL },

    { YSW_R3_C1, "70", YSW_MF_COMMAND, on_speaker_volume, 300, NULL },
    { YSW_R3_C2, "80", YSW_MF_COMMAND, on_speaker_volume, 400, NULL },
    { YSW_R3_C3, "90\n(Loud)", YSW_MF_COMMAND, on_speaker_volume, 500, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C4, "Off", YSW_MF_COMMAND, on_speaker_volume, 0, NULL },

    { 0, "Speaker Volume", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t synth_gain_menu[] = {
    { YSW_R1_C1, "10\n(Soft)", YSW_MF_COMMAND, on_synth_gain, 10, NULL },
    { YSW_R1_C2, "25", YSW_MF_COMMAND, on_synth_gain, 25, NULL },
    { YSW_R1_C3, "50", YSW_MF_COMMAND, on_synth_gain, 50, NULL },

    { YSW_R2_C1, "75", YSW_MF_COMMAND, on_synth_gain, 75, NULL },
    { YSW_R2_C2, "100\n(Normal)", YSW_MF_COMMAND, on_synth_gain, 100, NULL },
    { YSW_R2_C3, "200", YSW_MF_COMMAND, on_synth_gain, 200, NULL },

    { YSW_R3_C1, "300", YSW_MF_COMMAND, on_synth_gain, 300, NULL },
    { YSW_R3_C2, "400", YSW_MF_COMMAND, on_synth_gain, 400, NULL },
    { YSW_R3_C3, "500\n(Loud)", YSW_MF_COMMAND, on_synth_gain, 500, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Volume (%)", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t listen_menu[] = {
    { YSW_R2_C2, "Headphn\nVolume", YSW_MF_PLUS, ysw_menu_nop, 0, headphone_volume_menu },
    { YSW_R2_C3, "Speaker\nVolume", YSW_MF_PLUS, ysw_menu_nop, 0, speaker_volume_menu },

    { YSW_R3_C3, "Synth\nGain", YSW_MF_PLUS, ysw_menu_nop, 0, synth_gain_menu },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Listen", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t program_category_2_menu[] = {
    { YSW_R1_C1, "Reed", YSW_MF_PLUS, on_program_2, 8, program_template },
    { YSW_R1_C2, "Pipe", YSW_MF_PLUS, on_program_2, 9, program_template },
    { YSW_R1_C3, "Synth\nLead", YSW_MF_PLUS, on_program_2, 10, program_template },
    { YSW_R1_C4, "Synth\nPad", YSW_MF_PLUS, on_program_2, 11, program_template },

    { YSW_R2_C1, "Synth\nEffects", YSW_MF_PLUS, on_program_2, 12, program_template },
    { YSW_R2_C2, "Ethnic", YSW_MF_PLUS, on_program_2, 13, program_template },
    { YSW_R2_C3, "Perc", YSW_MF_PLUS, on_program_2, 14, program_template },
    { YSW_R2_C4, "Sound\nEffects", YSW_MF_PLUS, on_program_2, 15, program_template },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { YSW_BUTTON_1, "Type", YSW_MF_REPLACE, on_settings_chord_type_1, 0, chord_type_template },
    { YSW_BUTTON_2, "Root", YSW_MF_REPLACE, ysw_menu_nop, 0, root_menu },
    { YSW_BUTTON_3, "Program", YSW_MF_REPLACE, on_program_1, 0, program_category_1_menu },

    { 0, "More", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t program_category_1_menu[] = {
    { YSW_R1_C1, "Piano", YSW_MF_PLUS, on_program_2, 0, program_template },
    { YSW_R1_C2, "Chrom\nPerc", YSW_MF_PLUS, on_program_2, 1, program_template },
    { YSW_R1_C3, "Organ", YSW_MF_PLUS, on_program_2, 2, program_template },
    { YSW_R1_C4, "Guitar", YSW_MF_PLUS, on_program_2, 3, program_template },

    { YSW_R2_C1, "Bass", YSW_MF_PLUS, on_program_2, 4, program_template },
    { YSW_R2_C2, "Strings", YSW_MF_PLUS, on_program_2, 5, program_template },
    { YSW_R2_C3, "Ensem\nble", YSW_MF_PLUS, on_program_2, 6, program_template },
    { YSW_R2_C4, "Brass", YSW_MF_PLUS, on_program_2, 7, program_template },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C4, "More", YSW_MF_PLUS, ysw_menu_nop, 0, program_category_2_menu },

    { YSW_BUTTON_1, "Type", YSW_MF_REPLACE, on_settings_chord_type_1, 0, chord_type_template },
    { YSW_BUTTON_2, "Root", YSW_MF_REPLACE, ysw_menu_nop, 0, root_menu },
    { YSW_BUTTON_3, "Program", YSW_MF_REPLACE, on_program_1, 0, program_category_1_menu },

    { 0, "Category", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t settings_menu[] = {

    { YSW_R2_C1, "Melody\nProgram", YSW_MF_PLUS, on_program_1, 0, program_category_1_menu },
    { YSW_R2_C2, "Chord\nProgram", YSW_MF_PLUS, on_program_1, 1, program_category_1_menu },

    { YSW_R3_C1, "Chord\nType", YSW_MF_PLUS, on_settings_chord_type_1, 0, chord_type_template },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Settings", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t input_mode_menu[] = {
    { YSW_R1_C1, "Note", YSW_MF_COMMAND, on_mode_melody, 0, NULL },
    { YSW_R1_C2, "Chord", YSW_MF_COMMAND, on_mode_chord, 0, NULL },
    { YSW_R1_C3, "Rhythm", YSW_MF_COMMAND, on_mode_rhythm, 0, NULL },

    { YSW_R2_C1, "Harp", YSW_MF_COMMAND, on_mode_harp, 0, NULL },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Input Mode", YSW_MF_END, NULL, 0, NULL },
};

static const ysw_menu_item_t root_menu[] = {
    { YSW_R1_C1, "C", YSW_MF_PRESS, on_root, 0, NULL },
    { YSW_R1_C2, "D", YSW_MF_PRESS, on_root, 2, NULL },
    { YSW_R1_C3, "E", YSW_MF_PRESS, on_root, 4, NULL },
    { YSW_R1_C4, "F", YSW_MF_PRESS, on_root, 5, NULL },

    { YSW_R2_C1, "G", YSW_MF_PRESS, on_root, 7, NULL },
    { YSW_R2_C2, "A", YSW_MF_PRESS, on_root, 9, NULL },
    { YSW_R2_C3, "B", YSW_MF_PRESS, on_root, 11, NULL },

    { YSW_BUTTON_1, "Type", YSW_MF_REPLACE, on_settings_chord_type_1, 0, chord_type_template },
    { YSW_BUTTON_2, "Root", YSW_MF_REPLACE, ysw_menu_nop, 0, root_menu },
    { YSW_BUTTON_3, "Program", YSW_MF_REPLACE, on_program_1, 0, program_category_1_menu },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },

    { 0, "Menu Name", YSW_MF_END, NULL, 0, NULL },
};
const ysw_menu_item_t performer_menu[] = {
    { YSW_R1_C1, "Input\nMode", YSW_MF_PLUS, ysw_menu_nop, 0, input_mode_menu },
    { YSW_R1_C2, "Root", YSW_MF_REPLACE, ysw_menu_nop, 0, root_menu },

    { YSW_R2_C1, "Listen", YSW_MF_PLUS, ysw_menu_nop, 0, listen_menu },

    { YSW_R3_C1, "Close", YSW_MF_MINUS, on_close, 0, NULL },
    { YSW_R3_C3, "Settings", YSW_MF_PLUS, ysw_menu_nop, 0, settings_menu },

    { YSW_R4_C1, "Back", YSW_MF_MINUS, ysw_menu_nop, 0, NULL },
    { YSW_R4_C3, "Hide\nMenu", YSW_MF_TOGGLE, ysw_menu_nop, 0, NULL },

    { YSW_BUTTON_1, "Type", YSW_MF_REPLACE, on_settings_chord_type_1, 0, chord_type_template },
    { YSW_BUTTON_2, "Root", YSW_MF_REPLACE, ysw_menu_nop, 0, root_menu },
    { YSW_BUTTON_3, "Program", YSW_MF_REPLACE, on_program_1, 0, program_category_1_menu },

    { 0, "Music", YSW_MF_END, NULL, 0, NULL },
};

void ysw_performer_create(ysw_bus_t *bus, zm_music_t *music)
{
    assert(music);

    ysw_performer_t *performer = ysw_heap_allocate(sizeof(ysw_performer_t));

    performer->bus = bus;
    performer->music = music;
    performer->mode = YSW_PERFORMER_MODE_HARP;

    performer->chord_type = ysw_array_get(music->chord_types, DEFAULT_CHORD_TYPE);

    fire_bank_select(performer, RHYTHM_CHANNEL, YSW_MIDI_DRUM_BANK);
    fire_bank_select(performer, BACKGROUND_RHYTHM, YSW_MIDI_DRUM_BANK);

    performer->container = lv_obj_create(lv_scr_act(), NULL);
    assert(performer->container);

    ysw_style_editor(performer->container);

    lv_obj_set_size(performer->container, 320, 240);
    lv_obj_align(performer->container, NULL, LV_ALIGN_CENTER, 0, 0);

    performer->menu = ysw_menu_create(bus, performer_menu, ysw_app_softkey_map, performer);
    ysw_menu_add_glass(performer->menu, performer->container);

    performer->queue = ysw_app_create_queue();
    ysw_bus_subscribe(bus, YSW_ORIGIN_NOTE, performer->queue);
    ysw_bus_subscribe(bus, YSW_ORIGIN_SOFTKEY, performer->queue);
    ysw_bus_subscribe(bus, YSW_ORIGIN_SEQUENCER, performer->queue);

    ysw_app_handle_events(performer->queue, process_event, performer);

    ysw_bus_unsubscribe(bus, YSW_ORIGIN_NOTE, performer->queue);
    ysw_bus_unsubscribe(bus, YSW_ORIGIN_SOFTKEY, performer->queue);
    ysw_bus_unsubscribe(bus, YSW_ORIGIN_SEQUENCER, performer->queue);
    ysw_bus_delete_queue(bus, performer->queue);

    ysw_menu_free(performer->menu);
    lv_obj_del(performer->container);
    ysw_heap_free(performer);
}

