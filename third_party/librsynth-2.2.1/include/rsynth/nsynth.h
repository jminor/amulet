/* $Id: nsynth.h,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
*/

/*-------------------------------------------------------------------
 * function prototypes
 *-------------------------------------------------------------------*/
extern void parwave  PROTO((klatt_global_ptr, klatt_frame_ptr pars,short int *jwave));
extern void parwave_init  PROTO((klatt_global_ptr));
extern void pr_pars PROTO((void));
extern int  init_synth PROTO((rsynth_synth *synth, int argc,char *argv[]));
