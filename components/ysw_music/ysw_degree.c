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

static char *get_suffix(uint32_t number)
{
    char *suffix = "th";
    if (number < 4 || number > 20) {
        uint8_t x = number % 10;
        switch (x) {
            case 1:
                suffix = "st";
                break;
            case 2:
                suffix = "nd";
                break;
            case 3:
                suffix = "rd";
                break;
        }
    }
    return suffix;
}

static char *get_accidental_name(ysw_accidental_t accidental)
{
    char *accidental_name;
    if (accidental == YSW_ACCIDENTAL_FLAT) {
        accidental_name = "b";
    } else if (accidental == YSW_ACCIDENTAL_SHARP) {
        accidental_name = "#";
    } else {
        accidental_name = "";
    }
    return accidental_name;
}

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

/**
 * Convert a scale tonic, progression degree and chord_note into a midi note
 * @param scale_tonic midi note of scale tonic (e.g. 60)
 * @param root_number degree number of chord in progression (e.g. I, IV or V)
 * @param degree_number note in styled chord (e.g. 3 for 3rd or 0 for 7th an octave lower)
 * @param accidental flat, natural or sharp
 * @return midi note representing the parameters
 */
int8_t ysw_degree_to_note(uint8_t scale_tonic, uint8_t root_number, int8_t degree_number, ysw_accidental_t accidental)
{
    static uint8_t intervals[7][7] = {
        /* C */ { 0, 2, 4, 5, 7, 9, 11 },
        /* D */ { 0, 2, 3, 5, 7, 9, 10 },
        /* E */ { 0, 1, 3, 5, 7, 8, 10 },
        /* F */ { 0, 2, 4, 6, 7, 9, 11 },
        /* G */ { 0, 2, 4, 5, 7, 9, 10 },
        /* A */ { 0, 2, 3, 5, 7, 8, 10 },
        /* B */ { 0, 1, 3, 5, 6, 8, 10 },
    };

    int8_t root_offset = root_number - 1;
    int8_t root_octave = root_offset / 7;
    int8_t root_index = root_offset % 7;

    int8_t degree_octave = 0;
    uint8_t normalized_degree_number = 0;
    ysw_degree_normalize(degree_number, &normalized_degree_number, &degree_octave);
    uint8_t normalized_degree_index = normalized_degree_number - 1;

    int8_t root_interval = intervals[0][root_index];
    int8_t degree_interval = intervals[root_index][normalized_degree_index];

    int8_t note = scale_tonic +
        ((12 * root_octave) + root_interval) +
        ((12 * degree_octave) + degree_interval + accidental);

    ESP_LOGD(TAG, "scale_tonic=%d, root_number=%d, degree_number=%d, root_interval=%d, degree_interval=%d, accidental=%d, note=%d", scale_tonic, root_number, degree_number, root_interval, degree_interval, accidental, note);

    return note;
}

void ysw_degree_to_name(char *buffer, size_t buffer_size, int8_t degree_number, ysw_accidental_t accidental)
{
    assert(accidental >= YSW_ACCIDENTAL_FLAT && accidental <= YSW_ACCIDENTAL_SHARP);

    uint8_t normalized_degree_number; // in range 1 to 7
    int8_t octave; // in range -n to +n

    ysw_degree_normalize(degree_number, &normalized_degree_number, &octave);

    char *suffix = get_suffix(normalized_degree_number);
    char *label = get_accidental_name(accidental);
    if (octave) {
        snprintf(buffer, buffer_size, "%d%s %s (%+d)", normalized_degree_number, suffix, label, octave);
    } else {
        snprintf(buffer, buffer_size, "%d%s %s", normalized_degree_number, suffix, label);
    }
}

