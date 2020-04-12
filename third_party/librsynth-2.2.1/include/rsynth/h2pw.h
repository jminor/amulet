#ifdef  JONS_PARWAVE 
#include "rsynth/parwave.h"
extern klatt_frame_t def_pars;
extern klatt_global_t klatt_global;
extern int init_synth PROTO((int argc,char *argv[]));
#else
#include "rsynth/nsynth.h"
#endif
