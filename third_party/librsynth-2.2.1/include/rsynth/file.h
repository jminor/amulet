extern int file_init PROTO((rsynth_synth *synth, int argc,char *argv[]));

//-- now in rstruct.h
typedef void (*file_write_p) PROTO((rsynth_synth *synth,int n, short *data));
typedef void (*file_term_p) PROTO((rsynth_synth *synth));

//extern file_write_p file_write;
//extern file_term_p file_term;
