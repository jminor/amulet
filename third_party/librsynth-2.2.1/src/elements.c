#include <config.h>
/* $Id: elements.c,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
 */
char *elements_id = "$Id: elements.c,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $";
#include <stdio.h>
#include <math.h>

#include "proto.h"
#include "rstruct.h"

#include "elements.h"
#include "phfeat.h"

Elm_t Elements[] =
{
#include "Elements.def"
};

unsigned num_Elements = (sizeof(Elements) / sizeof(Elm_t));

char *Ep_name[nEparm] =
{
 "fn", "f1", "f2", "f3",
 "b1", "b2", "b3", "an",
 "a1", "a2", "a3", "a4",
 "a5", "a6", "ab", "av",
 "avc", "asp", "af"
};
