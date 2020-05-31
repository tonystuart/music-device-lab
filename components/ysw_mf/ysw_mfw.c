// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_mfw.h"

#include "ysw_csv.h"
#include "ysw_heap.h"
#include "ysw_mf.h"
#include "ysw_music.h"

#include "esp_log.h"
#include "hash.h"

#include "assert.h"
#include "errno.h"
#include "unistd.h"

#define TAG "YSW_MFW"

typedef struct {
    FILE *file;
    ysw_music_t *music;
    hash_t *cs_map;
    hash_t *hp_map;
} ysw_mfw_t;

static void write_cs(ysw_mfw_t *ysw_mfw)
{
    uint32_t cs_count = ysw_music_get_cs_count(ysw_mfw->music);
    for (uint32_t i = 0; i < cs_count; i++) {
        ysw_cs_t *cs = ysw_music_get_cs(ysw_mfw->music, i);
        char name[YSW_MF_MAX_NAME_LENGTH];
        ysw_csv_escape(cs->name, name, sizeof(name));
        fprintf(ysw_mfw->file, "%d,%d,%s,%d,%d,%d,%d,%d,%d\n",
                YSW_MF_CHORD_STYLE,
                i,
                name,
                cs->instrument,
                cs->octave,
                cs->mode,
                cs->transposition,
                cs->tempo,
                cs->divisions);

        printf("%d,%d,%s,%d,%d,%d,%d,%d,%d\n",
                YSW_MF_CHORD_STYLE,
                i,
                name,
                cs->instrument,
                cs->octave,
                cs->mode,
                cs->transposition,
                cs->tempo,
                cs->divisions);

        if (!hash_alloc_insert(ysw_mfw->cs_map, cs, (void *)i)) {
            ESP_LOGE(TAG, "hash_alloc_insert(cs_map) failed");
            abort();
        }

        uint32_t sn_count = ysw_cs_get_sn_count(cs);
        for (uint32_t j = 0; j < sn_count; j++) {
            ysw_sn_t *sn = ysw_cs_get_sn(cs, j);
            fprintf(ysw_mfw->file, "%d,%d,%d,%d,%d,%d\n",
                    YSW_MF_STYLE_NOTE,
                    sn->degree,
                    sn->velocity,
                    sn->start,
                    sn->duration,
                    sn->flags);

            printf("%d,%d,%d,%d,%d,%d\n",
                    YSW_MF_STYLE_NOTE,
                    sn->degree,
                    sn->velocity,
                    sn->start,
                    sn->duration,
                    sn->flags);
        }
    }
}

static void write_hp(ysw_mfw_t *ysw_mfw)
{
    uint32_t hp_count = ysw_music_get_hp_count(ysw_mfw->music);
    for (uint32_t i = 0; i < hp_count; i++) {
        ysw_hp_t *hp = ysw_music_get_hp(ysw_mfw->music, i);
        char name[YSW_MF_MAX_NAME_LENGTH];
        ysw_csv_escape(hp->name, name, sizeof(name));
        fprintf(ysw_mfw->file, "%d,%d,%s,%d,%d,%d,%d,%d\n",
                YSW_MF_HARMONIC_PROGRESSION,
                i,
                name,
                hp->instrument,
                hp->octave,
                hp->mode,
                hp->transposition,
                hp->tempo);

        printf("%d,%d,%s,%d,%d,%d,%d,%d\n",
                YSW_MF_HARMONIC_PROGRESSION,
                i,
                name,
                hp->instrument,
                hp->octave,
                hp->mode,
                hp->transposition,
                hp->tempo);

        if (!hash_alloc_insert(ysw_mfw->hp_map, hp, (void *)i)) {
            ESP_LOGE(TAG, "hash_alloc_insert(hp_map) failed");
            abort();
        }

        uint32_t ps_count = ysw_hp_get_ps_count(hp);
        for (uint32_t j = 0; j < ps_count; j++) {
            ysw_ps_t *ps = ysw_hp_get_ps(hp, j);
            hnode_t *node = hash_lookup(ysw_mfw->cs_map, ps->cs);
            if (!node) {
                ESP_LOGE(TAG, "hash_lookup(cs_map, cs=%p) failed", ps->cs);
                abort();
            }
            uint32_t cs_index = (uint32_t)hnode_get(node);
            fprintf(ysw_mfw->file, "%d,%d,%d,%d\n",
                    YSW_MF_PROGRESSION_STEP,
                    ps->degree,
                    cs_index,
                    ps->flags);

            printf("%d,%d,%d,%d\n",
                    YSW_MF_PROGRESSION_STEP,
                    ps->degree,
                    cs_index,
                    ps->flags);
        }
    }
}

void ysw_mfw_write_to_file(FILE *file, ysw_music_t *music)
{
    extern void hash_ensure_assert_off();
    hash_ensure_assert_off();

    ysw_mfw_t *ysw_mfw = &(ysw_mfw_t){};

    ysw_mfw->file = file;
    ysw_mfw->music = music;

    ysw_mfw->cs_map = hash_create(100, NULL, NULL);
    if (!ysw_mfw->cs_map) {
        ESP_LOGE(TAG, "hash_create cs_map failed");
        abort();
    }

    ysw_mfw->hp_map = hash_create(100, NULL, NULL);
    if (!ysw_mfw->hp_map) {
        ESP_LOGE(TAG, "hash_create hp_map failed");
        abort();
    }

    write_cs(ysw_mfw);
    write_hp(ysw_mfw);

    hash_free_nodes(ysw_mfw->cs_map);
    hash_free_nodes(ysw_mfw->hp_map);

    hash_destroy(ysw_mfw->cs_map);
    hash_destroy(ysw_mfw->hp_map);
}

void ysw_mfw_write(ysw_music_t *music)
{
    FILE *file = fopen(YSW_MUSIC_TEMP, "w");
    if (!file) {
        ESP_LOGE(TAG, "fopen file=%s failed, errno=%d", YSW_MUSIC_TEMP, errno);
        abort();
    }

    ysw_mfw_write_to_file(file, music);
    fclose(file);

    // spiffs doesn't provide atomic rename, see also ysw_mfr_read
    int rc = unlink(YSW_MUSIC_CSV);
    if (rc == -1) {
        ESP_LOGE(TAG, "unlink file=%s failed, errno=%d", YSW_MUSIC_CSV, errno);
        abort();
    }

    rc = rename(YSW_MUSIC_TEMP, YSW_MUSIC_CSV);
    if (rc == -1) {
        ESP_LOGE(TAG, "rename old=%s, new=%s failed, errno=%d", YSW_MUSIC_TEMP, YSW_MUSIC_CSV, errno);
        abort();
    }
}

