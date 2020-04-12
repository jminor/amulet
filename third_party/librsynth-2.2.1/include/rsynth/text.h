/* $Id: text.h,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
*/
extern int isvowel PROTO((int chr));
extern int isconsonant PROTO((int chr));

typedef void (*out_p) PROTO((void *arg,char *s));
extern int NRL PROTO((char *s,unsigned n,darray_ptr phone));

