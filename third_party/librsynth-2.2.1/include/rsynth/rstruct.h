/* -*- Mode: C -*- */

/*-------------------------------------------------------------------*\
 * File: rstruct.h
 * Author: Bryan Jurish
 *
 * Description:
 *   + new structures & typedefs for librsynth speech synthesizer
 *     library
\*-------------------------------------------------------------------*/

#ifndef _RSTRUCT_H

/*-------------------------------------------------------------------
 * local global #define's
 *-------------------------------------------------------------------*/
//#define RSYNTH_DEBUG
#undef RSYNTH_DEBUG

//#define RSYNTH_DICT_WARN_VERBOSE
#undef RSYNTH_DICT_WARN_VERBOSE


/*-------------------------------------------------------------------
 * imported global #define's
 *-------------------------------------------------------------------*/

//-- from hplay.c (linux-specific hack!)
#define SAMP_RATE 8000

//-- from aufile.c 
#define SUN_MAGIC 	0x2e736e64		/* Really '.snd' */
#define SUN_HDRSIZE	24			/* Size of minimal header */
#define SUN_UNSPEC	((unsigned)(~0))	/* Unspecified data size */
#define SUN_ULAW	1			/* u-law encoding */
#define SUN_LIN_8	2			/* Linear 8 bits */
#define SUN_LIN_16	3			/* Linear 16 bits */

//-- from nsynth.h
#define CASCADE_PARALLEL      1
#define ALL_PARALLEL          2
#define NPAR                 40
#define IMPULSIVE             1
#define NATURAL               2
#ifndef PI
#define PI            3.1415927
#endif

#ifndef TRUE
#define TRUE                  1
#endif

#ifndef FALSE
#define FALSE                 0
#endif


/*-------------------------------------------------------------------
 * global typedefs
 *-------------------------------------------------------------------*/

//-- from nsynth.h
typedef struct
 {
  long F0hz10;            /* Voicing fund freq in Hz                          */        
  long AVdb;              /* Amp of voicing in dB,            0 to   70       */        
  long F1hz;              /* First formant freq in Hz,        200 to 1300     */        
  long B1hz;              /* First formant bw in Hz,          40 to 1000      */        
  long F2hz;              /* Second formant freq in Hz,       550 to 3000     */        
  long B2hz;              /* Second formant bw in Hz,         40 to 1000      */        
  long F3hz;              /* Third formant freq in Hz,        1200 to 4999    */        
  long B3hz;              /* Third formant bw in Hz,          40 to 1000      */        
  long F4hz;              /* Fourth formant freq in Hz,       1200 to 4999    */        
  long B4hz;              /* Fourth formant bw in Hz,         40 to 1000      */        
  long F5hz;              /* Fifth formant freq in Hz,        1200 to 4999    */        
  long B5hz;              /* Fifth formant bw in Hz,          40 to 1000      */        
  long F6hz;              /* Sixth formant freq in Hz,        1200 to 4999    */        
  long B6hz;              /* Sixth formant bw in Hz,          40 to 2000      */        
  long FNZhz;             /* Nasal zero freq in Hz,           248 to  528     */        
  long BNZhz;             /* Nasal zero bw in Hz,             40 to 1000      */        
  long FNPhz;             /* Nasal pole freq in Hz,           248 to  528     */        
  long BNPhz;             /* Nasal pole bw in Hz,             40 to 1000      */        
  long ASP;               /* Amp of aspiration in dB,         0 to   70       */        
  long Kopen;             /* # of samples in open period,     10 to   65      */        
  long Aturb;             /* Breathiness in voicing,          0 to   80       */        
  long TLTdb;             /* Voicing spectral tilt in dB,     0 to   24       */        
  long AF;                /* Amp of frication in dB,          0 to   80       */        
  long Kskew;             /* Skewness of alternate periods,   0 to   40 in sample#/2  */
  long A1;                /* Amp of par 1st formant in dB,    0 to   80       */        
  long B1phz;             /* Par. 1st formant bw in Hz,       40 to 1000      */        
  long A2;                /* Amp of F2 frication in dB,       0 to   80       */        
  long B2phz;             /* Par. 2nd formant bw in Hz,       40 to 1000      */        
  long A3;                /* Amp of F3 frication in dB,       0 to   80       */        
  long B3phz;             /* Par. 3rd formant bw in Hz,       40 to 1000      */        
  long A4;                /* Amp of F4 frication in dB,       0 to   80       */        
  long B4phz;             /* Par. 4th formant bw in Hz,       40 to 1000      */        
  long A5;                /* Amp of F5 frication in dB,       0 to   80       */        
  long B5phz;             /* Par. 5th formant bw in Hz,       40 to 1000      */        
  long A6;                /* Amp of F6 (same as r6pa),        0 to   80       */        
  long B6phz;             /* Par. 6th formant bw in Hz,       40 to 2000      */        
  long ANP;               /* Amp of par nasal pole in dB,     0 to   80       */        
  long AB;                /* Amp of bypass fric. in dB,       0 to   80       */        
  long AVpdb;             /* Amp of voicing,  par in dB,      0 to   70       */        
  long Gain0;             /* Overall gain, 60 dB is unity,    0 to   60       */        
 } klatt_frame_t, *klatt_frame_ptr;

typedef struct 
 {
  int  synthesis_model;
  int  quiet_flag;
  int  f0_flutter;
  int  outsl;
  long samrate;
  long nfcascade;
  long glsource;
  long nspfr;
 } klatt_global_t, *klatt_global_ptr;


//-- from elements.h
enum Eparm_e
 {
  fn, f1, f2, f3, b1, b2, b3, an, a1, a2, a3, a4, a5, a6, ab, av, avc, asp, af,
  nEparm
 };


//-- from holmes.c
typedef struct
 {
  float v;                        /* boundary value */
  int t;                          /* transition time */
 } holmes_slope_t;

typedef struct
 {
  holmes_slope_t p[nEparm];
 } holmes_trans_t;

typedef struct
 {
  float a;
  float b;
  float v;
 } holmes_filter_t, *holmes_filter_ptr;


/*-------------------------------------------------------------------
 * new structures
 *-------------------------------------------------------------------*/

//-- rsynth_dict : dictionary structure
typedef struct _rsynth_dict {
  void *dict;
  char **dialect;
  char *dict_path;
} rsynth_dict;


//-- flags for rsynth_synth
#define RSY_USEAUDIO 1
#define RSY_RUNNING 2

//-- rsynth_synth: synthesizer-global data structure
typedef struct _rsynth_synth {
  //-- OUTPUT
  int flags;
  int verbose;   /* verbosity level (currently only 0 or 1) */
  int help_only; /* this gets set when an arg is '-help': really a
		    hack to allow us to keep using Nick's 'getarg.c'
		 */

  //-- AUDIO: properties
  long samp_rate; // sample rate (Hz)
  unsigned au_encoding;  // black magic
  unsigned au_size;      // ... ditto

  //-- AUDIO: fds
  int dev_fd;            // fd to audio output device
  int linear_fd;         // fd to raw-linear output file
  int au_fd;             // fd to sun/next .au ulaw output file

  //-- AUDIO: filenames
  char *dev_filename;    // audio device
  char *linear_filename; // raw linear output file
  char *au_filename;     // sun/next output file

  //-- AUDIO: functions
  void (*file_write);  // really a 'void (*file_write_p)'
  void (*file_term);   // really a 'void (*file_term_p)'
                       // -> use like this: '(*(file_write_p)synth->file_write)(ARGS);'

  //-- SYNTH: Klatt guts
  klatt_frame_t def_pars;      // default values for pars array from .doc file
  klatt_global_t klatt_global; // ye olde fynthefyzer guttf
  double mSec_per_frame;
  int impulse;
  int casc;
  
  //-- HOLMES (tts/prosidy guts?)
  int speed;
  char *par_name;
  char *jsru_name;
  FILE *par_file;
  FILE *jsru_file;
  double frac;

  //-- DICTIONARY
  rsynth_dict rsdict;

  //-- PHONETICS (do we need this instance-specific?)
  //trie_ptr phtoelm;

} rsynth_synth, *rsynth_synth_ptr;


/*-------------------------------------------------------------------
 * new prototypes: struct manipulators
 *-------------------------------------------------------------------*/

/*
 * synth = rsynth_new();
 *   + create & initialize a new synth object with default values
 *   + uses rsynth_initialize() to init the new synth
 */
extern rsynth_synth *rsynth_new PROTO((void));

/*
 * rsynth_initialize(synth);
 *   + initialize a synth object with default values
 *   + returns boolean
 */
extern int rsynth_initialize PROTO((rsynth_synth *synth));


/*
 * rsynth_clear(synth);
 *   + "private" pre-destructor: frees substructure
 */
extern void rsynth_clear PROTO((rsynth_synth *synth));

/*
 * rsynth_destroy(synth);
 *   + destroy the synth: frees memory
 */
extern void rsynth_destroy PROTO((rsynth_synth *synth));



/* rsynth_configure(synth,argc,argv);
 *     + configures synth according to options in argc,argv
 */
extern int rsynth_configure PROTO((rsynth_synth *synth, int argc, char **argv));

/* rsynth_start(synth)
 *   + starts the synth, return is boolean
 */
extern int rsynth_start PROTO((rsynth_synth *synth));

/* rsynth_stop(synth)
 *   + stops a running synth, return is boolean
 */
extern int rsynth_stop PROTO((rsynth_synth *synth));

/* rsynth_sync(synth)
 *   + syncs a running synth, return is boolean
 */
extern int rsynth_sync PROTO((rsynth_synth *synth));


//-- audio
extern int rsynth_start_audio PROTO((rsynth_synth *synth));
extern int rsynth_stop_audio PROTO((rsynth_synth *synth));
extern int rsynth_sync_audio PROTO((rsynth_synth *synth));

//-- klatt synth
extern int rsynth_start_synth PROTO((rsynth_synth *synth));
extern int rsynth_stop_synth PROTO((rsynth_synth *synth));

//-- holmes
extern int rsynth_start_holmes PROTO((rsynth_synth *synth));
extern int rsynth_stop_holmes PROTO((rsynth_synth *synth));

//-- dict
extern int rsynth_start_dict PROTO((rsynth_synth *synth));
extern int rsynth_stop_dict PROTO((rsynth_synth *synth));

//-- file
extern int rsynth_start_file PROTO((rsynth_synth *synth));
extern int rsynth_stop_file PROTO((rsynth_synth *synth));
extern int rsynth_sync_file PROTO((rsynth_synth *synth));



#define _RSYNTH_H
#endif /* _RSYNTH_H */
