// Inspired by HxCModPlayer by Jean François DEL NERO

#include "hxcmod.h"


#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "assert.h"

#define TAG "HXCMOD"

#define PERIOD_TABLE_LENGTH  MAXNOTES
#define FULL_PERIOD_TABLE_LENGTH  (PERIOD_TABLE_LENGTH * 8)

#ifdef HXCMOD_BIGENDIAN_MACHINE
#define GET_BGI_W(big_endian_word) (big_endian_word)
#else
#define GET_BGI_W(big_endian_word) ((big_endian_word >> 8) | ((big_endian_word&0xFF) << 8))
#endif

static const short periodtable[] = {
    27392, 25856, 24384, 23040, 21696, 20480, 19328, 18240, 17216, 16256, 15360, 14496,
    13696, 12928, 12192, 11520, 10848, 10240,  9664,  9120,  8606,  8128,  7680,  7248,
    6848,  6464,  6096,  5760,  5424,  5120,  4832,  4560,  4304,  4064,  3840,  3624,
    3424,  3232,  3048,  2880,  2712,  2560,  2416,  2280,  2152,  2032,  1920,  1812,
    1712,  1616,  1524,  1440,  1356,  1280,  1208,  1140,  1076,  1016,   960,   906,
    856,   808,   762,   720,   678,   640,   604,   570,   538,   508,   480,   453,
    428,   404,   381,   360,   339,   320,   302,   285,   269,   254,   240,   226,
    214,   202,   190,   180,   170,   160,   151,   143,   135,   127,   120,   113,
    107,   101,    95,    90,    85,    80,    75,    71,    67,    63,    60,    56,
    53,    50,    47,    45,    42,    40,    37,    35,    33,    31,    30,    28,
    27,    25,    24,    22,    21,    20,    19,    18,    17,    16,    15,    14,
    13,    13,    12,    11,    11,    10,     9,     9,     8,     8,     7,     7
};

static SemaphoreHandle_t semaphore;

static void enter_critical_section()
{
    xSemaphoreTake(semaphore, portMAX_DELAY);
}

static void leave_critical_section()
{
    xSemaphoreGive(semaphore);
}

static void memcopy(void * dest, void *source, unsigned long size)
{
    unsigned long i;
    unsigned char * d,*s;

    d = (unsigned char*)dest;
    s = (unsigned char*)source;
    for (i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

static void memclear(void * dest, unsigned char value, unsigned long size)
{
    unsigned long i;
    unsigned char * d;

    d = (unsigned char*)dest;
    for (i = 0; i < size; i++) {
        d[i] = value;
    }
}

static int getnote(modcontext * mod, unsigned short period, int finetune)
{
    for (int i = 0; i < FULL_PERIOD_TABLE_LENGTH; i++) {
        if (period >= mod->fullperiod[i]) {
            return i;
        }
    }
    return MAXNOTES;
}

static void worknote(note *nptr, channel *cptr, modcontext *mod)
{
    muint sample = (nptr->sampperiod & 0xF0) | (nptr->sampeffect >> 4);
    muint period = ((nptr->sampperiod & 0xF) << 8) | nptr->period;

    if (!period && !sample) {
        return;
    }

    cptr->sampnum = sample - 1;
    cptr->sampdata = mod->sampledata[cptr->sampnum];
    cptr->length = GET_BGI_W(mod->song.samples[cptr->sampnum].length);
    cptr->reppnt = GET_BGI_W(mod->song.samples[cptr->sampnum].reppnt);
    cptr->replen = GET_BGI_W(mod->song.samples[cptr->sampnum].replen);
    cptr->finetune = (mod->song.samples[cptr->sampnum].finetune) & 0xF;
    cptr->volume = mod->song.samples[cptr->sampnum].volume;
    cptr->samppos = 0;

    if (cptr->finetune) {
        if (cptr->finetune <= 7) {
            period = mod->fullperiod[getnote(mod,period,0) + cptr->finetune];
        }
        else {
            period = mod->fullperiod[getnote(mod,period,0) - (16 - (cptr->finetune)) ];
        }
    }

    cptr->period = period;
}

void hxcmod_init(modcontext * modctx)
{
    assert(modctx);

    memclear(modctx,0,sizeof(modcontext));
    modctx->playrate = 44100;
    modctx->stereo = 1;
    modctx->stereo_separation = 1;
    modctx->filter = 1;

    for (muint i = 0; i < PERIOD_TABLE_LENGTH - 1; i++) {
        for (muint j = 0; j < 8; j++) {
            modctx->fullperiod[(i*8) + j] = periodtable[i] - (((periodtable[i] - periodtable[i+1]) / 8) * j);
        }
    }

    semaphore = xSemaphoreCreateMutex();
}

void hxcmod_load(modcontext * modctx, void * mod_data, int mod_data_size)
{
    assert(modctx);
    assert(mod_data);
    assert(mod_data_size);

    unsigned char *modmemory = (unsigned char *)mod_data;
    unsigned char *endmodmemory = modmemory + mod_data_size;

    memcopy(&(modctx->song.title),modmemory,1084);
    modctx->number_of_channels = 4;
    modmemory += 1084;

    assert(modmemory < endmodmemory);

    for (muint i = 0, max = 0; i < 128; i++) {
        while (max <= modctx->song.patterntable[i]) {
            modmemory += 256 * modctx->number_of_channels;
            max++;
            assert(modmemory < endmodmemory);
        }
    }

    for (muint i = 0; i < 31; i++) {
        modctx->sampledata[i] = 0;
    }

    sample *sptr = modctx->song.samples;

    for (muint i = 0; i < 31; i++, sptr++) {
        if (sptr->length == 0) {
            continue;
        }

        modctx->sampledata[i] = (mchar *)modmemory;
        modmemory += (GET_BGI_W(sptr->length)*2);

        if (GET_BGI_W(sptr->replen) + GET_BGI_W(sptr->reppnt) > GET_BGI_W(sptr->length)) {
            sptr->replen = GET_BGI_W((GET_BGI_W(sptr->length) - GET_BGI_W(sptr->reppnt)));
        }

        assert(modmemory <= endmodmemory);
    }

    modctx->sampleticksconst = ((3546894UL * 16) / modctx->playrate) << 6; //8448*428/playrate;

    for (muint i = 0; i < modctx->number_of_channels; i++) {
        modctx->channels[i].volume = 0;
        modctx->channels[i].period = 0;
    }

    modctx->mod_loaded = 1;
}

void hxcmod_fillbuffer(modcontext * modctx, msample * outbuffer, mssize nbsample)
{
    assert(modctx);
    assert(outbuffer);

    enter_critical_section();

    if (!modctx->mod_loaded) {
        for (mssize i = 0; i < nbsample; i++)
        {
            *outbuffer++ = 0;
            *outbuffer++ = 0;
        }
        return;
    }

    int last_left = modctx->last_l_sample;
    int last_right = modctx->last_r_sample;

    for (mssize i = 0; i < nbsample; i++) {

        int left = 0;
        int right = 0;

        channel *cptr = modctx->channels;

        for (muint j = 0; j < modctx->number_of_channels; j++, cptr++) {
            if (cptr->period != 0) {
                cptr->samppos += cptr->sampinc;

                if (cptr->replen < 2) {
                    if ((cptr->samppos >> 11) >= cptr->length) {
                        cptr->length = 0;
                        cptr->reppnt = 0;
                        cptr->samppos = 0;
                    }
                } else {
                    if ((cptr->samppos >> 11) >= (unsigned long)(cptr->replen + cptr->reppnt)) {
                        if (cptr->sampdata) {
                            cptr->samppos = ((unsigned long)(cptr->reppnt)<<11) + (cptr->samppos % ((unsigned long)(cptr->replen + cptr->reppnt)<<11));
                        }
                    }
                }

                unsigned long k = cptr->samppos >> 10;

#if 0
                if (cptr->samppos) {
                    ESP_LOGD(TAG, "period=%d, sampinc=%d, samppos=%ld, samppos>>11=%d, k=%d, length=%d", cptr->period, cptr->sampinc, cptr->samppos, cptr->samppos >> 11, k, cptr->length);
                }
#endif

                if (cptr->sampdata) {
                    if (!(j & 3) || ((j & 3) == 3)) {
                        left += (cptr->sampdata[k] * cptr->volume);
                    } else {
                        right += (cptr->sampdata[k] * cptr->volume);
                    }
                }

            }
        }

        int temp_left = (short)left;
        int temp_right = (short)right;

        if (modctx->filter) {
            left = (left + last_left) >> 1;
            right = (right + last_right) >> 1;
        }

        if (modctx->stereo_separation == 1) {
            left = (left + (right >> 1));
            right = (right + (left >> 1));
        }

        if (left > 32767) {
            left = 32767;
        }
        if (left < -32768) {
            left = -32768;
        }
        if (right > 32767) {
            right = 32767;
        }
        if (right < -32768) {
            right = -32768;
        }

        *outbuffer++ = left;
        *outbuffer++ = right;

        last_left = temp_left;
        last_right = temp_right;

    }

    modctx->last_l_sample = last_left;
    modctx->last_r_sample = last_right;

    leave_critical_section();
}

void play_note(modcontext *modctx, int period)
{
    note n = {
        .sampperiod = (period >> 8) & 0x0f,
        .period = period & 0xff,
        .sampeffect = 16,
        .effect = 0,
    };

    enter_critical_section();
    channel *cptr = &modctx->channels[1];
    worknote(&n, cptr, modctx);
    if (cptr->period != 0) {
        short finalperiod = cptr->period;
        if (finalperiod) {
            //ESP_LOGD(TAG, "play note final_period=%d", finalperiod);
            cptr->sampinc = ((modctx->sampleticksconst) / finalperiod);
        }
        else {
            cptr->sampinc = 0;
        }
    } else {
        cptr->sampinc = 0;
    }

    //ESP_LOGD(TAG, "play_note sampinc=%ld", cptr->sampinc);
    leave_critical_section();
}

void hxcmod_unload(modcontext * modctx)
{
    assert(modctx);
    memclear(&modctx->song,0,sizeof(modctx->song));
    memclear(&modctx->sampledata,0,sizeof(modctx->sampledata));
    memclear(modctx->channels,0,sizeof(modctx->channels));
    modctx->sampleticksconst = 0;
    modctx->number_of_channels = 0;
    modctx->mod_loaded = 0;
    modctx->last_r_sample = 0;
    modctx->last_l_sample = 0;
}
