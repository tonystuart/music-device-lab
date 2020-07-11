// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_degree.h"

#include "ysw_midi.h"

#include "esp_log.h"
#include "assert.h"
#include "stdio.h"

#define TAG "YSW_DEGREE"

// See https://en.wikipedia.org/wiki/Degree_(music)

// "In set theory, for instance, the 12 degrees of the chromatic scale usually are
// numbered starting from C=0, the twelve pitch classes being numbered from 0 to 11."

// See https://en.wikipedia.org/wiki/Roman_numeral_analysis

const char *ysw_degree_roman_choices =
"I\n"
"II\n"
"III\n"
"IV\n"
"V\n"
"VI\n"
"VII";

const char *ysw_degree_roman[] = {
    "I",
    "II",
    "III",
    "IV",
    "V",
    "VI",
    "VII",
};

#define YSW_DEGREE_SZ (sizeof(ysw_degree_roman) / sizeof(const char *))

const char *ysw_degree_cardinal_choices =
"1st\n"
"2nd\n"
"3rd\n"
"4th\n"
"5th\n"
"6th\n"
"7th";

const char *ysw_degree_cardinal[] = {
    "1st",
    "2nd",
    "3rd",
    "4th",
    "5th",
    "6th",
    "7th",
};

const uint8_t ysw_degree_intervals[7][7] = {
    /* C */ { 0, 2, 4, 5, 7, 9, 11 },
    /* D */ { 0, 2, 3, 5, 7, 9, 10 },
    /* E */ { 0, 1, 3, 5, 7, 8, 10 },
    /* F */ { 0, 2, 4, 6, 7, 9, 11 },
    /* G */ { 0, 2, 4, 5, 7, 9, 10 },
    /* A */ { 0, 2, 3, 5, 7, 8, 10 },
    /* B */ { 0, 1, 3, 5, 6, 8, 10 },
};

void ysw_degree_normalize(int8_t degree_number, uint8_t *normalized_degree_number, int8_t *octave)
{
    if (degree_number < 1) {
        if (octave) {
            *octave = (degree_number / YSW_MIDI_UNPO) - 1;
        }
        degree_number = YSW_MIDI_UNPO + (degree_number % YSW_MIDI_UNPO);
        uint8_t degree_index = degree_number - 1;
        if (normalized_degree_number) {
            *normalized_degree_number = (degree_index % YSW_MIDI_UNPO) + 1;
        }
    } else {
        uint8_t degree_index = degree_number - 1;
        if (octave) {
            *octave = degree_index / YSW_MIDI_UNPO;
        }
        if (normalized_degree_number) {
            *normalized_degree_number = (degree_index % YSW_MIDI_UNPO) + 1;
        }
    }
}

int8_t ysw_degree_denormalize(uint8_t normalized_degree_number, int8_t octave)
{
    uint8_t normalized_degree_index = normalized_degree_number - 1;
    int8_t new_degree_index = (octave * YSW_MIDI_UNPO) + normalized_degree_index;
    int8_t new_degree_number = new_degree_index + 1;
    ESP_LOGD(TAG, "input degree=%d, octave=%d, output degree=%d", normalized_degree_number, octave, new_degree_number);
    return new_degree_number;
}

int8_t ysw_degree_set_octave(int8_t degree_number, int8_t relative_octave)
{
    uint8_t normalized_degree = 0;
    int8_t normalized_octave = 0;
    ysw_degree_normalize(degree_number, &normalized_degree, &normalized_octave);

    int8_t correction = 0;

    if (relative_octave < 0) {
        correction = 1;
    } else if (relative_octave == 0) {
        correction = 0;
    } else {
        correction = -1;
    }

    int8_t octave_notes = relative_octave * (YSW_MIDI_UNPO + 1);
    uint8_t new_degree_number = octave_notes + normalized_degree + correction;
    return new_degree_number;
}

/**
 * Convert a scale tonic, hp degree and chord_note into a midi note
 * @param scale_tonic midi note of scale tonic (e.g. 60)
 * @param root_number degree number of chord in hp (e.g. I, IV or V)
 * @param degree_number note in styled chord (e.g. 3 for 3rd or 0 for 7th an octave lower)
 * @param accidental flat, natural or sharp
 * @return midi note representing the parameters
 */
uint8_t ysw_degree_to_note(uint8_t scale_tonic, uint8_t root_number, int8_t degree_number, ysw_accidental_t accidental)
{
    int8_t root_offset = root_number - 1;
    int8_t root_octave = root_offset / 7;
    int8_t root_index = root_offset % 7;

    int8_t degree_octave = 0;
    uint8_t normalized_degree_number = 0;
    ysw_degree_normalize(degree_number, &normalized_degree_number, &degree_octave);
    uint8_t normalized_degree_index = normalized_degree_number - 1;

    int8_t root_interval = ysw_degree_intervals[0][root_index];
    int8_t degree_interval = ysw_degree_intervals[root_index][normalized_degree_index];

    uint8_t note = scale_tonic +
        ((12 * root_octave) + root_interval) +
        ((12 * degree_octave) + degree_interval + accidental);

    //ESP_LOGD(TAG, "ysw_degree_to_note scale_tonic=%d", scale_tonic);
    //ESP_LOGD(TAG, "ysw_degree_to_note root_number=%d", root_number);
    //ESP_LOGD(TAG, "ysw_degree_to_note degree_number=%d", degree_number);
    //ESP_LOGD(TAG, "ysw_degree_to_note accidental=%d", accidental);
    //ESP_LOGD(TAG, "ysw_degree_to_note root_octave=%d", root_octave);
    //ESP_LOGD(TAG, "ysw_degree_to_note normalized_degree_number=%d", normalized_degree_number);
    //ESP_LOGD(TAG, "ysw_degree_to_note degree_octave=%d", degree_octave);
    //ESP_LOGD(TAG, "ysw_degree_to_note root_index=%d", root_index);
    //ESP_LOGD(TAG, "ysw_degree_to_note normalized_degree_index=%d", normalized_degree_index);
    //ESP_LOGD(TAG, "ysw_degree_to_note root_interval=%d", root_interval);
    //ESP_LOGD(TAG, "ysw_degree_to_note degree_interval=%d", degree_interval);
    //ESP_LOGD(TAG, "ysw_degree_to_note note=%d", note);

    return note;
}

const char* ysw_degree_get_name(uint8_t degree)
{
    uint8_t degree_index = to_index(degree);
    assert(degree_index < YSW_DEGREE_SZ);
    return ysw_degree_roman[degree_index];
}

