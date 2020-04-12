/* $Id: say.h,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
*/

extern unsigned xlate_string PROTO((rsynth_synth *synth,char *string,darray_ptr phone));
extern void say_string PROTO((rsynth_synth *synth,char *s));
extern char *concat_args PROTO((int argc,char *argv[]));
extern int suspect_word PROTO((rsynth_synth *synth,char *s,int n));
extern void say_phones PROTO((rsynth_synth *synth,char *phone,int len, int verbose));
extern unsigned spell_out PROTO((rsynth_synth *synth,char *word,int n,darray_ptr phone));
extern unsigned xlate_ordinal PROTO((rsynth_synth *synth,long int value,darray_ptr phone));
extern unsigned xlate_cardinal PROTO((rsynth_synth *synth,long int value,darray_ptr phone));

