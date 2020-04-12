/* $Id: holmes.h,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
*/
extern unsigned holmes PROTO((rsynth_synth *synth,unsigned nelm,unsigned char *elm,unsigned nsamp, short *samp_base));
extern int init_holmes PROTO((rsynth_synth *synth,int argc,char *argv[]));
extern void term_holmes PROTO((rsynth_synth *synth));
//extern int speed;
