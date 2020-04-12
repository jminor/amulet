/* -*- Mode: C -*- */

/*-------------------------------------------------------------------*\
 * File: rsynth.h
 * Author: Bryan Jurish
 *
 * Description:
 *   + Top-level header file for shared library 'librsynth'
 *   + adapted from Nick Ing-Simmons' 'rsynth' speech synthesizer
 *   + for the guts, see file "rsynth/rstruct.h"
\*-------------------------------------------------------------------*/

#ifndef _RSYNTH_H

#include "rsynth/useconfig.h"

#ifdef STDC_HEADERS
# include <ctype.h>
# include <stdio.h>
# include <stdarg.h>
# include <stddef.h>
# include <stdlib.h>
#endif

#ifdef HAVE_MATH_H
# include <math.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_SYS_FILE_H
# include <sys/file.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_MEMORY_H
# include <memory.h>
#endif

#ifdef HAVE_MALLOC_H
# include <malloc.h>
#endif

#ifdef HAVE_LIBGDBM
# include <gdbm.h>
#endif

#ifdef HAVE_SYS_IOCCOM_H
# include <sys/ioccom.h>
#endif

#ifdef HAVE_SYS_AUDIOIO_H
# include <sys/audioio.h>
#endif

#ifdef HAVE_SUN_AUDIOIO_H
# include <sys/audioio.h>
#endif

#ifdef HAVE_LIBC_H
# include <libc.h>
#endif

#ifdef HAVE_AUDIO_AUDIOLIB_H
# include <audio/audiolib.h>
#endif

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#ifdef HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif


#include "rsynth/proto.h"
#include "rsynth/rstruct.h"

#include "rsynth/nsynth.h"
#include "rsynth/hplay.h"
#include "rsynth/dict.h"
#include "rsynth/ASCII.h"
#include "rsynth/darray.h"
#include "rsynth/holmes.h"
#include "rsynth/phtoelm.h"
#include "rsynth/getargs.h"
#include "rsynth/text.h"
#include "rsynth/phones.h"
#include "rsynth/file.h"
#include "rsynth/say.h"


#define _RSYNTH_H
#endif /* _RSYNTH_H */
