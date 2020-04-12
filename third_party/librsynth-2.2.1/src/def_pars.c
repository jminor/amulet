#include <config.h>
/* $Id: def_pars.c,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
 */
char *def_pars_id = "$Id: def_pars.c,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $";
#include <stdio.h>
#include <useconfig.h>
#include <math.h>
#include "proto.h"

#include "rstruct.h"

#include "getargs.h"
#include "nsynth.h"
#include "hplay.h"

//-- now a part of the 'rsynth' structure!
//klatt_global_t klatt_global;

/* default values for pars array from .doc file */
klatt_frame_t synth_def_pars =
{
#include "pars.def"
};

#ifdef USE_RC_FILE

#define RC_FILE "/.sayrc";

/* Based on an idea by 

   John Cartmill   -  cartmill@wisconsin.nrlssc.navy.mil 

   The ability to read the user's  .sayrc file 
   ( which has the same format as  the pars.def file) 

   Not enabled by default as I don't have one other than 
   pars.def! to test it on and it does not handle comments  

 */

static void
read_rc_file(void)
{
 char *home = getenv("HOME");
 if (home)
  {
   char *path = malloc(strlen(home) + strlen(RC_FILE) + 1);
   if (path)
    {
     FILE *f = fopen(strcat(strcpy(path, home), RC_FILE), "r");
     if (f)
      {
       char string[256];
       int i;
       union
        {
         klatt_t def_pars;
         long array[NPAR];
        }
       pars;
       for (i = 0; i < NPAR; ++i)
        {
         fgets(string, sizeof(string), infp);
         value = atoi(string);
         pars.array[i] = value;
        }
       def_pars = pars.def_pars;
       fclose(f);
      }
     free(path);
    }
  }
}

#endif

int
init_synth(synth, argc, argv)
rsynth_synth *synth;
int argc;
char *argv[];
{
#ifdef USE_RC_FILE
 read_rc_file();
#endif

 argc = getargs(&(synth->help_only),
		"Synth paramters",argc, argv,
                "q", NULL, &(synth->klatt_global.quiet_flag), "Quiet - minimal messages",
                "I", NULL, &(synth->impulse),                 "Impulse glottal source",
                "c", "%d", &(synth->casc),                    "Number cascade formants",
                "F", "%d", &(synth->klatt_global.f0_flutter), "F0 flutter",
                "f", "%lg", &(synth->mSec_per_frame),         "mSec per frame",
                "t", "%d", &(synth->def_pars.TLTdb),          "Tilt dB",
                "x", "%d", &(synth->def_pars.F0hz10),         "Base F0 in 0.1Hz",
                NULL);

 rsynth_start_synth(synth);

 return argc;
}


/*------------------------------------------------------------------
 * start
 *------------------------------------------------------------------*/
int
rsynth_start_synth(synth)
rsynth_synth *synth;
{
  synth->klatt_global.samrate = synth->samp_rate;

  if (synth->casc > 0)
    {
      synth->klatt_global.synthesis_model = CASCADE_PARALLEL;
      synth->klatt_global.nfcascade = synth->casc;
    }
  else
    synth->klatt_global.synthesis_model = ALL_PARALLEL;

  if (synth->impulse)
    synth->klatt_global.glsource = IMPULSIVE;

  synth->klatt_global.nspfr = (synth->klatt_global.samrate * synth->mSec_per_frame) / 1000;
#ifdef HAVE_NONSTDARITH
  /* turn off strict IEEE compliance in favour of speed */
  nonstandard_arithmetic();
#endif

  return 1;
}


/*------------------------------------------------------------------
 * stop
 *------------------------------------------------------------------*/
int rsynth_stop_synth(synth)
     rsynth_synth *synth;
{
  return 1;
}
