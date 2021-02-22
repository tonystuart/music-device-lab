// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_csv.h"
#include "ysw_heap.h"
#include "ysw_string.h"
#include "fluidsynth.h"
#include "fluid_defsfont.h"
#include "fluid_synth.h"
#include "stdio.h"
#include "stdlib.h"

// See http://www.synthfont.com/Tutorial6.html
// See http://www.synthfont.com/SFSPEC21.PDF

static const int iz_gen_x[] = {
    GEN_FINETUNE,
    GEN_OVERRIDEROOTKEY,
};

#define IZ_GEN_SZ (sizeof(iz_gen_x) / sizeof(iz_gen_x[0]))

static void extract_sample(char *ofn, FILE *csvf, fluid_inst_zone_t *iz, int sx)
{
    fluid_sample_t *sample = fluid_inst_zone_get_sample(iz);
    int length = sample->end - sample->start;
    printf("keylo=%d, keyhi=%d, vello=%d, velhi=%d, name=%s, start=%d, end=%d, length=%d\n",
            iz->range.keylo, iz->range.keyhi,
            iz->range.vello, iz->range.velhi,
            sample->name, sample->start, sample->end, length);

    int loop_start = (sample->loopstart - sample->start) / 2; // divide by 2 for int8_t
    int loop_end = (sample->loopend - sample->start) / 2;
    int loop_length = loop_end - loop_start - 1;

    char sn[64];
    ysw_csv_escape(sample->name, sn, sizeof(sn));

    fprintf(csvf, "1,%d,%s,%d,%d,60,0", sx, sn, loop_start, loop_length);

    for (int i = 0; i < IZ_GEN_SZ; i++) {
        fluid_gen_t *gen = &iz->gen[iz_gen_x[i]];
        if (gen->flags) {
            fprintf(csvf, ",%g", gen->val);
        } else {
            fprintf(csvf, ",");
        }
    }

    fprintf(csvf, "\n");

    char sfn[128];
    snprintf(sfn, sizeof(sfn), "%s/samples/%s", ofn, sn);

    FILE *sf = fopen(sfn, "w");
    assert(sf);
    for (int i = sample->start; i < sample->end; i++) {
        int8_t s8data = (int8_t)((sample->data[i] >> 8) & 0xff);
        size_t wc = fwrite(&s8data, 1, 1, sf);
        assert(wc == 1);
    }
    fclose(sf);
}

static void extract_ranges(char *ofn, FILE *csvf, fluid_inst_t *inst, int *sx)
{
    fluid_inst_zone_t *iz = fluid_inst_get_zone(inst);
    while (iz) {
        fprintf(csvf, "3,%d,%d,%d,%d,%d\n",
                iz->range.keylo, iz->range.keyhi,
                iz->range.vello, iz->range.velhi, *sx);
        iz = fluid_inst_zone_next(iz);
        (*sx)++;
    }
}

static const int inst_gen_x[] = {
    GEN_VOLENVDELAY,
    GEN_VOLENVATTACK,
    GEN_VOLENVHOLD,
    GEN_VOLENVDECAY,
    GEN_VOLENVSUSTAIN,
    GEN_VOLENVRELEASE,
    GEN_ATTENUATION,
};

#define INST_GEN_SZ (sizeof(inst_gen_x) / sizeof(inst_gen_x[0]))

static void extract_program(FILE *csvf, fluid_defpreset_t *dp, fluid_inst_t *inst, int px)
{
    char preset_name[64];
    ysw_csv_escape(dp->name, preset_name, sizeof(preset_name));
    int length = strlen(preset_name);
    while (length > 0 && preset_name[--length] == ' ') {
        preset_name[length] = 0;
    }

    fprintf(csvf, "2,%d,%s,0,%d", px, preset_name, dp->num);

    for (int i = 0; i < INST_GEN_SZ; i++) {
        fluid_gen_t *gen = &inst->global_zone->gen[inst_gen_x[i]];
        if (gen->flags) {
            fprintf(csvf, ",%g", gen->val);
        } else {
            fprintf(csvf, ",");
        }
    }

    fprintf(csvf, "\n");
}

static void extract_samples(char *ofn, FILE *csvf, fluid_inst_t *inst, int sx)
{
    fluid_inst_zone_t *iz = fluid_inst_get_zone(inst);
    while (iz) {
        extract_sample(ofn, csvf, iz, sx);
        iz = fluid_inst_zone_next(iz);
        sx++;
    }
}

static void write_comment(FILE *csvf)
{
    fprintf(csvf, "#type(1=sample),index,name,reppnt,replen,volume,pan,fine-tune,root-key\n");
    fprintf(csvf, "#type(2=program),index,name,type,gm,delay,attack,hold,decay,sustain,release,attenuation\n");
    fprintf(csvf, "#type(3=patch),from-note,to-note,from-vel,to-vel,sample\n");
}

static void extract_preset(char *ofn, FILE *csvf, fluid_preset_t *preset, int *px, int *sx)
{
    fluid_defpreset_t *dp = fluid_preset_get_data(preset);
    fluid_preset_zone_t *pz = fluid_defpreset_get_zone(dp);

    while (pz) {
        fluid_inst_t *inst = fluid_preset_zone_get_inst(pz);
        write_comment(csvf);

        extract_samples(ofn, csvf, inst, *sx);
        extract_program(csvf, dp, inst, *px);
        extract_ranges(ofn, csvf, inst, sx);

        pz = fluid_preset_zone_next(pz);
        (*px)++;
    }
}

static void extract_all_presets(char *ofn, FILE *csvf, fluid_sfont_t *sfont, int *px, int *sx)
{
    fluid_defsfont_t *defsfont = fluid_sfont_get_data(sfont);
    for (fluid_list_t *list = defsfont->preset; list != NULL; list = fluid_list_next(list))
    {
        fluid_preset_t *preset = (fluid_preset_t *)fluid_list_get(list);
        extract_preset(ofn, csvf, preset, px, sx);
    }
}

static FILE *initialize_output(char *ofn)
{
    if (access(ofn, F_OK) != -1) {
        fprintf(stderr, "folder %s already exists", ofn);
        exit(2);
    }

    int rc = mkdir(ofn, 0755);
    assert(rc != -1);

    char samples[128];
    snprintf(samples, sizeof(samples), "%s/samples", ofn);
    rc = mkdir(samples, 0755);
    assert(rc != -1);

    char csv[128];
    snprintf(csv, sizeof(csv), "%s/extract.csv", ofn);
    FILE *csvf = fopen(csv, "w");
    assert(csvf);

    return csvf;
}

static fluid_sfont_t *load_soundfont(char *sffn)
{
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth = new_fluid_synth(settings);

    int sfont_id = fluid_synth_sfload(synth, sffn, 1);
    assert(sfont_id != -1);

    fluid_list_t *list = synth->sfont;
    fluid_sfont_t *sfont = fluid_list_get(list);

    return sfont;
}

int extract(int argc, char *argv[])
{
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: %s soundfont-file-name output-folder-name [num]", argv[0]);
        exit(1);
    }

    fluid_sfont_t *sfont = load_soundfont(argv[1]);
    FILE *csvf = initialize_output(argv[2]);

    int px = 0;
    int sx = 0;

    if (argc == 4) {
        fluid_preset_t *preset = fluid_sfont_get_preset(sfont, 0, 0);
        extract_preset(argv[2], csvf, preset, &px, &sx);
    } else {
        extract_all_presets(argv[2], csvf, sfont, &px, &sx);
    }

    fclose(csvf);

    return 0;
}
