/* -*- Mode: C -*- */

/*-------------------------------------------------------------------*\
 * File: rstruct.c
 * Author: Bryan Jurish
 *
 * Description: synth-structure manipulators
\*-------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "proto.h"
#include "useconfig.h"
#include "rstruct.h"

#include "nsynth.h"
#include "hplay.h"
#include "dict.h"
#include "ASCII.h"
#include "darray.h"
#include "holmes.h"
#include "phtoelm.h"
#include "getargs.h"
#include "text.h"
#include "phones.h"
#include "file.h"
#include "say.h"


extern klatt_frame_t synth_def_pars;
extern char *ph_am[];
extern char *rsynth_Revision;
extern char *default_dev_filename;

#define RSYNTH_CLEARPTR(ptr) if (ptr) { free(ptr); ptr = NULL; }

/*------------------------------------------------------------------
 * construction
 *------------------------------------------------------------------*/
rsynth_synth *rsynth_new(void)
{
  rsynth_synth *synth;
  synth = (rsynth_synth *)malloc(sizeof(rsynth_synth));
  if (!synth) {
    perror("librsynth: unable to allocate synthesizer");
    return NULL;
  }
  if (rsynth_initialize(synth)) return synth;
  else {
    perror("librsynth: unable to initialize synthesizer");
    free(synth);
    return NULL;
  }
}

/*------------------------------------------------------------------
 * pre-destructor
 *------------------------------------------------------------------*/
void
rsynth_clear(synth)
     rsynth_synth *synth;
{
  // stop running first...
  rsynth_stop(synth);

  // free filename strings
  RSYNTH_CLEARPTR(synth->dev_filename);
  RSYNTH_CLEARPTR(synth->linear_filename);
  RSYNTH_CLEARPTR(synth->au_filename);
  RSYNTH_CLEARPTR(synth->par_name);
  RSYNTH_CLEARPTR(synth->jsru_name);

  // free dictionary substructure
  if (synth->rsdict.dialect &&
      synth->rsdict.dialect != ph_am &&
      synth->rsdict.dialect != ph_br)
    {
      RSYNTH_CLEARPTR(synth->rsdict.dialect);
    }
  RSYNTH_CLEARPTR(synth->rsdict.dict_path);
}


/*------------------------------------------------------------------
 * destructor
 *------------------------------------------------------------------*/
void
rsynth_destroy(synth)
     rsynth_synth *synth;
{
  // stop running & clear substructure
  rsynth_clear(synth);

  // free the rest of the structure
  free(synth);
}

/*------------------------------------------------------------------
 * initialization
 *------------------------------------------------------------------*/
int rsynth_initialize(synth)
     rsynth_synth *synth;
{
  //-- OUTPUT
  synth->flags = 0;
  synth->verbose = 0;
  synth->help_only = 0;

  //-- AUDIO : properties
  synth->samp_rate = SAMP_RATE;
  synth->au_encoding = SUN_ULAW;
  synth->au_size = 0;

  //-- AUDIO : filenames
  synth->dev_filename = strdup(default_dev_filename);
  synth->linear_filename = NULL;
  synth->au_filename = NULL;

  //-- AUDIO : fds
  synth->dev_fd = -1;
  synth->linear_fd = -1;
  synth->au_fd = -1;

  //-- AUDIO : functions
  synth->file_write = NULL;
  synth->file_term = NULL;

  //-- SYNTH: Klatt guts
  synth->def_pars = synth_def_pars;
  synth->klatt_global.quiet_flag = TRUE;
  synth->klatt_global.glsource = NATURAL;
  synth->klatt_global.f0_flutter = 0;
  synth->mSec_per_frame = 10;
  synth->impulse = 0;
  synth->casc = 0;

  //-- HOLMES (tts guts?)
  synth->speed = 1;
  synth->par_name = NULL;
  synth->jsru_name = NULL;
  synth->par_file = NULL;
  synth->jsru_file = NULL;
  synth->frac = 1.0;

  //-- DICTIONARY
  synth->rsdict.dict = NULL;
  synth->rsdict.dialect = ph_am;
  synth->rsdict.dict_path = NULL;

  //-- PHONETICS
  //synth->phtoelm = NULL;

  return 1;
}


/*------------------------------------------------------------------
 * configuration
 *------------------------------------------------------------------*/
int
rsynth_configure(synth, argc, argv)
rsynth_synth *synth;
int argc;
char **argv;
 {

   argc = getargs(&(synth->help_only),
		  "Misc",argc, argv,
		  "v", NULL, &synth->verbose, "Verbose, show phonetic form",
		  NULL);

   argc = audio_init(synth, argc, argv);
   argc = init_synth(synth, argc, argv);
   argc = init_holmes(synth, argc, argv);
   argc = dict_init(synth, argc, argv);
   argc = file_init(synth, argc, argv);

   return argc;
 };


/*------------------------------------------------------------------
 * start
 *------------------------------------------------------------------*/
int rsynth_start (synth)
     rsynth_synth *synth;
{
  if (synth->flags & RSY_RUNNING) return 1;

  if (rsynth_start_audio(synth) &&
      rsynth_start_synth(synth) &&
      rsynth_start_holmes(synth) &&
      rsynth_start_dict(synth) &&
      rsynth_start_file(synth))
    {
      synth->flags |= RSY_RUNNING;
      return 1;
    }
  rsynth_stop(synth);
  return 0;
}

/*------------------------------------------------------------------
 * stop
 *------------------------------------------------------------------*/
int rsynth_stop (synth)
     rsynth_synth *synth;
{
  if (!(synth->flags & RSY_RUNNING)) return 1;

  if (rsynth_stop_audio(synth) &&
      rsynth_stop_synth(synth) &&
      rsynth_stop_holmes(synth) &&
      rsynth_stop_dict(synth) &&
      rsynth_stop_file(synth)) {
    synth->flags &= ~RSY_RUNNING;
    return 1;
  }
  return 0;
}

/*------------------------------------------------------------------
 * sync
 *------------------------------------------------------------------*/
int rsynth_sync (synth)
     rsynth_synth *synth;
{
  if (!(synth->flags & RSY_RUNNING)) return 1;
  
  return (rsynth_sync_audio(synth) &&
	  //rsynth_sync_synth(synth) &&
	  //rsynth_sync_holmes(synth) &&
	  //rsynth_sync_dict(synth) &&
	  rsynth_sync_file(synth));
}
