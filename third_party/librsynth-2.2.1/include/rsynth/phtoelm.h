/* $Id: phtoelm.h,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
*/
extern int phone_append PROTO((darray_ptr p,int ch));
unsigned phone_to_elm PROTO((rsynth_synth *synth, char *s, int n, darray_ptr elm));

