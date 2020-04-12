/* $Id: dict.h,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
*/
//extern void *dict;
extern unsigned char *dict_find PROTO((rsynth_dict *rsdict, char *s, unsigned n));
extern int dict_init PROTO((rsynth_synth *synth,int argc,char *argv[]));
extern int rsdict_init PROTO((rsynth_dict *rsdict, int *help_only,int argc,char *argv[]));
extern void dict_term PROTO((rsynth_dict *rsdict));
//extern char **dialect;

