
// Inspired by HxCModPlayer by Jean François DEL NERO

// Basic type
typedef unsigned char   muchar;
typedef signed   char   mchar;
typedef unsigned short  muint;
typedef          short  mint;
typedef unsigned long   mulong;

typedef unsigned long   mssize;
typedef signed short   msample;

#define NUMMAXCHANNELS 32
#define MAXNOTES 12*12

//
// MOD file structures
//

#pragma pack(1)

typedef struct {
	muchar  name[22];
	muint   length;
	muchar  finetune;
	muchar  volume;
	muint   reppnt;
	muint   replen;
} sample;

typedef struct {
	muchar  sampperiod;
	muchar  period;
	muchar  sampeffect;
	muchar  effect;
} note;

typedef struct {
	muchar  title[20];
	sample  samples[31];
	muchar  length;
	muchar  protracker;
	muchar  patterntable[128];
	muchar  signature[4];
	muchar  speed; // TONY: formerly set to 6, but was unused
} module;

#pragma pack()

//
// HxCMod Internal structures
//
typedef struct {
	mchar * sampdata;
	muint   length;
	muint   reppnt;
	muint   replen;
	muchar  sampnum;
	mulong  samppos;
	mulong  sampinc;
	muint   period;
	muchar  volume;
	muchar  finetune;
} channel;

typedef struct {
	module  song;
	mchar * sampledata[31];
	mulong  playrate;
	mulong  tick_cnt;
	mulong  sampleticksconst;
	channel channels[NUMMAXCHANNELS];
	muint   number_of_channels;
	muint   fullperiod[MAXNOTES * 8];
	muint   mod_loaded;
	mint    last_r_sample;
	mint    last_l_sample;
	mint    stereo;
	mint    stereo_separation;
	mint    filter;
} modcontext;

void  hxcmod_init( modcontext * modctx );
void  hxcmod_setcfg( modcontext * modctx, int samplerate, int stereo_separation, int filter );
muchar hxcmod_loadchannel(modcontext *modctx, sample *sample, void *data);
void hxcmod_fillbuffer( modcontext * modctx, msample * outbuffer, mssize nbsample);
void hxcmod_unload( modcontext * modctx );

