// Inspired by HxCModPlayer by Jean François DEL NERO

#pragma once

#ifndef HXCMOD_SLOW_TARGET
	#define HXCMOD_STATE_REPORT_SUPPORT 1
	#define HXCMOD_OUTPUT_FILTER 1
	#define HXCMOD_OUTPUT_STEREO_MIX 1
	#define HXCMOD_CLIPPING_CHECK 1
#endif

// Basic type
typedef unsigned char   muchar;
typedef signed   char   mchar;
typedef unsigned short  muint;
typedef          short  mint;
typedef unsigned long   mulong;

#ifdef HXCMOD_16BITS_TARGET
	typedef unsigned short  mssize;
#else
	typedef unsigned long   mssize;
#endif

#ifdef HXCMOD_8BITS_OUTPUT
	#ifdef HXCMOD_UNSIGNED_OUTPUT
	typedef unsigned char  msample;
	#else
	typedef signed char    msample;
	#endif
#else
	#ifdef HXCMOD_UNSIGNED_OUTPUT
	typedef unsigned short msample;
	#else
	typedef signed short   msample;
	#endif
#endif

#ifdef HXCMOD_MAXCHANNELS
	#define NUMMAXCHANNELS HXCMOD_MAXCHANNELS
#else
	#define NUMMAXCHANNELS 32
#endif

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
	muchar  speed;
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

	mchar * nxt_sampdata;
	muint   nxt_length;
	muint   nxt_reppnt;
	muint   nxt_replen;
	muchar  update_nxt_repeat;

	mchar * dly_sampdata;
	muint   dly_length;
	muint   dly_reppnt;
	muint   dly_replen;
	muchar  note_delay;

	mchar * lst_sampdata;
	muint   lst_length;
	muint   lst_reppnt;
	muint   lst_replen;
	muchar  retrig_cnt;
	muchar  retrig_param;

	muint   funkoffset;
	mint    funkspeed;

	mint    glissando;

	mulong  samppos;
	mulong  sampinc;
	muint   period;
	muchar  volume;
	muchar  effect;
	muchar  parameffect;
	muint   effect_code;

	mint    decalperiod;
	mint    portaspeed;
	mint    portaperiod;
	mint    vibraperiod;
	mint    Arpperiods[3];
	muchar  ArpIndex;

	muchar  volumeslide;

	muchar  vibraparam;
	muchar  vibrapointeur;

	muchar  finetune;

	muchar  cut_param;

	muint   patternloopcnt;
	muint   patternloopstartpoint;
} channel;

typedef struct {
	module  song;
	mchar * sampledata[31];
	note *  patterndata[128];

#ifdef HXCMOD_16BITS_TARGET
	muint   playrate;
#else
	mulong  playrate;
#endif

	muint   tablepos;
	muint   patternpos;
	muint   patterndelay;
	muchar  jump_loop_effect;
	muchar  bpm;

#ifdef HXCMOD_16BITS_TARGET
	muint   patternticks;
	muint   patterntickse;
	muint   patternticksaim;
	muint   patternticksem;
	muint   tick_cnt;
#else
	mulong  patternticks;
	mulong  patterntickse;
	mulong  patternticksaim;
	mulong  patternticksem;
	mulong  tick_cnt;
#endif

	mulong  sampleticksconst;

	channel channels[NUMMAXCHANNELS];

	muint   number_of_channels;

	muint   fullperiod[MAXNOTES * 8];

	muint   mod_loaded;

	mint    last_r_sample;
	mint    last_l_sample;

	mint    stereo;
	mint    stereo_separation;
	mint    bits;
	mint    filter;

#ifdef EFFECTS_USAGE_STATE
	int effects_event_counts[32];
#endif

} modcontext;

//
// Player states structures
//
typedef struct track_state_
{
	unsigned char instrument_number;
	unsigned short cur_period;
	unsigned char  cur_volume;
	unsigned short cur_effect;
	unsigned short cur_parameffect;
}track_state;

typedef struct tracker_state_
{
	int number_of_tracks;
	int bpm;
	int speed;
	int cur_pattern;
	int cur_pattern_pos;
	int cur_pattern_table_pos;
	unsigned int buf_index;
	track_state tracks[NUMMAXCHANNELS];
}tracker_state;

typedef struct tracker_state_instrument_
{
	char name[22];
	int active;
}tracker_state_instrument;

typedef struct tracker_buffer_state_
{
	int nb_max_of_state;
	int nb_of_state;
	int cur_rd_index;
	int sample_step;
	char name[64];
	tracker_state_instrument instruments[31];
	tracker_state * track_state_buf;
}tracker_buffer_state;

void  hxcmod_init( modcontext * modctx );
void  hxcmod_setcfg( modcontext * modctx, int samplerate, int stereo_separation, int filter );
void  hxcmod_load( modcontext * modctx, void * mod_data, int mod_data_size );
void hxcmod_fillbuffer( modcontext * modctx, msample * outbuffer, mssize nbsample);
void hxcmod_unload( modcontext * modctx );

