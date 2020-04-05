// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "esp_log.h"
#include "ysw_ble_synthesizer.h"
#include "ysw_sequencer.h"
#include "ysw_message.h"
#include "ysw_clip.h"

#define TAG "MAIN"

static QueueHandle_t synthesizer_queue;
static QueueHandle_t sequencer_queue;

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

#define TWELVE_BAR_BLUES 1

void app_main()
{
    esp_log_level_set("BLEServer", ESP_LOG_INFO);
    esp_log_level_set("BLEDevice", ESP_LOG_INFO);
    esp_log_level_set("BLECharacteristic", ESP_LOG_INFO);

    ysw_clip_t *s = ysw_clip_create();

#ifdef TWELVE_BAR_BLUES
    ysw_clip_add_chord_note(s, ysw_chord_note_create(1, 100, 0, 230));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(3, 80, 250, 230));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(5, 80, 500, 230));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(6, 80, 750, 230));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(7, 100, 1000, 230));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(6, 80, 1250, 230));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(5, 80, 1500, 230));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(3, 80, 1750, 230));

    ysw_clip_set_chord_duration(s, 2000);
    ysw_clip_set_instrument(s, 32);
    ysw_clip_set_tonic(s, 36);

    ysw_clip_add_chord(s, I);
    ysw_clip_add_chord(s, I);
    ysw_clip_add_chord(s, I);
    ysw_clip_add_chord(s, I);
    ysw_clip_add_chord(s, IV);
    ysw_clip_add_chord(s, IV);
    ysw_clip_add_chord(s, I);
    ysw_clip_add_chord(s, I);
    ysw_clip_add_chord(s, V);
    ysw_clip_add_chord(s, IV);
    ysw_clip_add_chord(s, I);
    ysw_clip_add_chord(s, I);
#endif

#ifdef LET_IT_BE
    ysw_clip_add_chord_note(s, ysw_chord_note_create(1, 100, 0, 1000));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(3, 80, 50, 1000));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(5, 80, 100, 1000));

    ysw_clip_add_chord_note(s, ysw_chord_note_create(1, 100, 1100, 500));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(3, 80, 1150, 500));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(5, 80, 1200, 500));

    ysw_clip_add_chord_note(s, ysw_chord_note_create(1, 100, 1700, 250));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(3, 80, 1750, 250));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(5, 80, 1800, 250));

    ysw_clip_add_chord_note(s, ysw_chord_note_create(5, 100, 2050, 250));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(3, 80, 2100, 250));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(1, 80, 2150, 250));
    ysw_clip_set_chord_duration(s, 2400);

    ysw_clip_set_instrument(s, 25);
    ysw_clip_set_tonic(s, 48);

    // Verse

    ysw_clip_add_chord(s, I);
    ysw_clip_add_chord(s, V);

    ysw_clip_add_chord(s, vi);
    ysw_clip_add_chord(s, IV);

    ysw_clip_add_chord(s, I);
    ysw_clip_add_chord(s, V);

    ysw_clip_add_chord(s, IV);
    ysw_clip_add_chord(s, I);

    // Repeat

    ysw_clip_add_chord(s, I);
    ysw_clip_add_chord(s, V);

    ysw_clip_add_chord(s, vi);
    ysw_clip_add_chord(s, IV);

    ysw_clip_add_chord(s, I);
    ysw_clip_add_chord(s, V);

    ysw_clip_add_chord(s, IV);
    ysw_clip_add_chord(s, I);

    // Chorus

    ysw_clip_add_chord(s, vi);
    ysw_clip_add_chord(s, V);

    ysw_clip_add_chord(s, IV);
    ysw_clip_add_chord(s, I);

    ysw_clip_add_chord(s, I);
    ysw_clip_add_chord(s, V);

    ysw_clip_add_chord(s, IV);
    ysw_clip_add_chord(s, I);
#endif

    ysw_clip_set_percent_tempo(s, 100);
    note_t *notes = ysw_clip_get_notes(s);
    assert(notes);

    synthesizer_queue = ysw_ble_synthesizer_create_task();

    ysw_sequencer_config_t config = {
        .on_note_on = on_note_on,
        .on_note_off = on_note_off,
        .on_program_change = on_program_change,
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
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    message = (ysw_sequencer_message_t){
        .type = YSW_SEQUENCER_PLAY,
            .play.loop_count = YSW_SEQUENCER_LOOP_REPEATEDLY,
    };

    ysw_message_send(sequencer_queue, &message);

    for (;;) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


