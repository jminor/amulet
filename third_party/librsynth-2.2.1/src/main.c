#include <config.h>
/* $Id: main.c,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
   $Log: main.c,v $
   Revision 1.1.1.1  2002/05/15 00:36:40  moocow
   # release 2.1.3 -- new directory structure

   Revision 1.4  2002/05/13 22:43:13  moocow
   # Version 2.1.2
   # + continued librarification; some makefile tuning
   # + renamed some types to jive with perl namespace

   Revision 1.3  2002/05/11 22:32:01  moocow
   + extracted globals from holmes.c

   Revision 1.2  2002/05/11 21:14:20  moocow
   + added audio file information and klatt synth to 'rsynth_synth'
     structure; removed globals

   Revision 1.1.1.1  2002/05/09 11:52:51  moocow
   + this is, and i mean it, the initial cvs import


 * Revision 1.13  1994/11/08  13:30:50  a904209
 * 2.0 release
 *
 * Revision 1.12  1994/11/04  13:32:31  a904209
 * 1.99.1 - Change configure stuff
 *
 * Revision 1.11  1994/11/02  10:55:31  a904209
 * Add autoconf. Tested on SunOS/Solaris
 *
 * Revision 1.10  1994/10/04  17:12:50  a904209
 * 3rd pre-release
 *
 * Revision 1.9  1994/10/04  09:08:27  a904209
 * Next Patch merge
 *
 * Revision 1.8  1994/10/03  08:41:47  a904209
 * 2nd pre-release                                                         
 *
 * Revision 1.7  1994/09/19  15:48:29  a904209
 * Split hplay.c, gdbm dictionary, start of f0 contour, netaudio and HP ports
 *
 * Revision 1.6  1994/04/15  16:47:37  a904209
 * Edits for Solaris2.3 (aka SunOs 5.3)
 *
 * Revision 1.5  1994/02/24  15:03:05  a904209
 * Added contributed linux, NeXT and SGI ports.
 *
 * Revision 1.4  93/11/18  16:29:06  a904209
 * Migrated nsyth.c towards Jon's scheme - merge still incomplete
 * 
 * Revision 1.3  93/11/16  14:32:44  a904209
 * Added RCS Ids, partial merge of Jon's new klatt/parwave
 *
 * Revision 1.3  93/11/16  14:00:58  a904209
 * Add IDs and merge Jon's klatt sources - incomplete
 *
 */

char *say_id = "$Id: main.c,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $";
extern char *rsynth_Revision;

/* shared with 'say.c' */
#include <stdio.h>
#include <ctype.h>
#include <useconfig.h>
#include <math.h>
#include "proto.h"

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

char *program = "say";

rsynth_synth *synth;

/* shared */
extern int darray_fget PROTO((FILE * f, darray_ptr p));
extern void say_file PROTO((rsynth_synth *synth, FILE * f));

/* main only */
int main PROTO((int argc, char *argv[], char *env[]));

int
main(argc, argv, env)
int argc;
char *argv[];
char *env[];        
{
 /* initialization */
 synth = rsynth_new();
 if (!synth) {
   fprintf(stderr, "could not initialize rsynth object!");
   exit(1);
 }

 /* arguments */
 program = argv[0];

 argc = rsynth_configure(synth,argc,argv);

 if (synth->help_only)
  {
   fprintf(stderr,"Usage: %s [options as above] words to say\n",program);
   fprintf(stderr,"or     %s [options as above] < file-to-say\n",program);
   fprintf(stderr,"(%s)\n",rsynth_Revision);
  }

 else
  {
   if (argc > 1)    
    {               
     char *s = concat_args(argc, argv);
     if (s)         
      {             
       say_string(synth,s);
       free(s);     
      }             
    }               
   else             
    say_file(synth,stdin);
  }
 if (synth->file_term)
  (*(file_term_p)synth->file_term)(synth);
 audio_term(synth);
 term_holmes(synth);
 if (synth->rsdict.dict)
  dict_term(&(synth->rsdict));
 return (0);
}
