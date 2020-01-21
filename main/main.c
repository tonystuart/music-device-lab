// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_clip.h"

void app_main()
{
    ysw_clip_t *s = ysw_clip_create();

    ysw_clip_add_chord_note(s, ysw_chord_note_create(TONIC, 100, 0, 230));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(MEDIANT, 80, 250, 230));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(DOMINANT, 80, 500, 230));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(SUBMEDIANT, 80, 750, 230));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(SUBTONIC, 100, 1000, 230));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(SUBMEDIANT, 80, 1250, 230));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(DOMINANT, 80, 1500, 230));
    ysw_clip_add_chord_note(s, ysw_chord_note_create(MEDIANT, 80, 1750, 230));

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

    ysw_clip_set_measure_duration(s, 2000);
    ysw_clip_set_percent_tempo(s, 100);
    ysw_clip_set_instrument(s, 0);
    note_t *notes = ysw_clip_get_notes(s);
    assert(notes);
}


