#include <config.h>

/*****************************************************************/
/***                                                           ***/
/***    Play out a file on Linux                               ***/
/***                                                           ***/
/***                H.F. Silverman 1/4/91                      ***/
/***    Modified:   H.F. Silverman 1/16/91 for amax parameter  ***/
/***    Modified:   A. Smith 2/14/91 for 8kHz for klatt synth  ***/
/***    Modified:   Rob W. W. Hooft (hooft@EMBL-Heidelberg.DE) ***/
/***                adapted for linux soundpackage Version 2.0 ***/
/***                                                           ***/
/*****************************************************************/

#include <useconfig.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/signal.h>

#include <sys/ioctl.h>

#include <sys/soundcard.h>
#include "proto.h"

#include "rsynth.h"

#include "getargs.h"
#include "hplay.h"


char *prog = "hplay";
char *default_dev_filename = "/dev/dsp";


static int
audio_open(rsynth_synth *synth)
{
 synth->dev_fd = open(synth->dev_filename, O_WRONLY | O_NDELAY);
 if (synth->dev_fd < 0)
  {
   fprintf(stderr, "librsynth: error opening audio device '%s': %s",
	   synth->dev_filename, strerror(errno));
   return 0;
  }
 return 1;
}

int
audio_init(rsynth_synth *synth, int argc, char *argv[])
{
 int rate_set = 0;
 int use_audio = 1;

 if (argv) { prog = argv[0]; } /* HACK! */

 argc = getargs(&synth->help_only,
		"linux Audio",argc, argv,
                "r", "%d", &rate_set,    "Sample rate",
                "a", NULL, &use_audio,   "Audio enable",
		"A", default_dev_filename, &(synth->dev_filename), "Audio device filename",
                NULL);

 if (synth->help_only)
  return argc;

 if (use_audio) {
   synth->flags |= RSY_USEAUDIO;
 }

 rsynth_start_audio(synth);

 return argc;
}


/*------------------------------------------------------------------
 * start
 *------------------------------------------------------------------*/
int
rsynth_start_audio(synth)
     rsynth_synth * synth;
{
  if (synth->flags & RSY_USEAUDIO) {
    audio_open(synth);

    if (synth->dev_fd > 0)
      {
	ioctl(synth->dev_fd, SNDCTL_DSP_SPEED, &(synth->samp_rate));
	if (synth->verbose) {
	  fprintf(stderr, "Actual sound rate: %ld\n", synth->samp_rate);
	}
	return 1;
      }
    return 0;
  }
  return 1;
}

void
//audio_term()
audio_term(rsynth_synth *synth)
{
  rsynth_stop_audio(synth);
}


/*------------------------------------------------------------------
 * sync
 *------------------------------------------------------------------*/
int
rsynth_sync_audio(rsynth_synth *synth)
{
  int dummy;

  /* Sync audio system  */
  if (synth->dev_fd >= 0) {
# ifdef RSYNTH_DEBUG
    fprintf(stderr, "librsynth: syncing audio fd %d\n",synth->dev_fd);
#  endif
    ioctl(synth->dev_fd, SNDCTL_DSP_SYNC, &dummy);
  }

  return 1;
}

/*------------------------------------------------------------------
 * stop
 *------------------------------------------------------------------*/
int
rsynth_stop_audio(rsynth_synth *synth)
{
  // sync audio system
  rsynth_sync_audio(synth);

  // ... and close it
  if (synth->dev_fd >= 0) {
    close(synth->dev_fd);
    synth->dev_fd = -1;
  }

 return 1;
}



void
//audio_play(int n, short *data)
audio_play(rsynth_synth *synth, int n, short *data)
{
 if (n > 0)
  {
   unsigned char *converted = (unsigned char *) malloc(n);
   int i;

   if (converted == NULL)
    {
     fprintf(stderr, "librsynth: Could not allocate memory for conversion\n");
     exit(3);
    }

   for (i = 0; i < n; i++)
    converted[i] = (data[i] - 32768) / 256;

   if (synth->dev_fd >= 0)
    {
     if (write(synth->dev_fd, converted, n) != n)
      perror("librsynth: error writing to audio device");
    }

   free(converted);
  }
}


