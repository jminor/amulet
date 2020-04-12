/* $Id: elements.h,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
*/
typedef struct
 {float stdy;
  float fixd;
  char  prop;
  char  ed;
  char  id;
 } interp_t, *interp_ptr;

/* -- now in rstruct.h: enum EParm_e -- */

extern char *Ep_name[nEparm];

typedef struct Elm_s
 {
  char *name;
  char rk;
  char du;
  char ud;
  unsigned char font;
  char  *dict;
  char  *ipa;
  long  feat;
  interp_t p[nEparm];
 } Elm_t, *Elm_ptr;

extern Elm_t Elements[];
extern unsigned num_Elements;


