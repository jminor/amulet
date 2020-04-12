/* $Id: hplay.h,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
*/
extern char *program;
//extern long samp_rate;
extern int audio_init PROTO((rsynth_synth *synth, int argc, char *argv[]));
extern void audio_term PROTO((rsynth_synth *synth));
extern void audio_play PROTO((rsynth_synth *synth, int n, short *data));

