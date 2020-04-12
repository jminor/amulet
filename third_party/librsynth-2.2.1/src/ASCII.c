#include <config.h>
/* $Id: ASCII.c,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
 */
char *ASCII_id = "$Id: ASCII.c,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $";
#include <stdio.h>
#include "ASCII.h"
char *ASCII[] =
{
 "null", "", "", "",
 "", "", "", "",
 "", "", "", "",
 "", "", "", "",
 "", "", "", "",
 "", "", "", "",
 "", "", "", "",
 "", "", "", "",
 "space", "exclamation mark", "double quote", "hash",
 "dollar", "percent", "ampersand", "quote",
 "open parenthesis", "close parenthesis", "asterisk", "plus",
 "comma", "minus", "full stop", "slash",
 "zero", "one", "two", "three",
 "four", "five", "six", "seven",
 "eight", "nine", "colon", "semi colon",
 "less than", "equals", "greater than", "question mark",
#ifndef ALPHA_IN_DICT
 "at", "ay", "bee", "see",
 "dee", "e", "eff", "gee",
 "aych", "i", "jay", "kay",
 "ell", "em", "en", "ohe",
 "pee", "kju", "are", "es",
 "tee", "you", "vee", "double you",
 "eks", "why", "zed", "open bracket",
#else                             /* ALPHA_IN_DICT */
 "at", "A", "B", "C",
 "D", "E", "F", "G",
 "H", "I", "J", "K",
 "L", "M", "N", "O",
 "P", "Q", "R", "S",
 "T", "U", "V", "W",
 "X", "Y", "Z", "open bracket",
#endif                            /* ALPHA_IN_DICT */
 "back slash", "close bracket", "circumflex", "underscore",
#ifndef ALPHA_IN_DICT
 "back quote", "ay", "bee", "see",
 "dee", "e", "eff", "gee",
 "aych", "i", "jay", "kay",
 "ell", "em", "en", "ohe",
 "pee", "kju", "are", "es",
 "tee", "you", "vee", "double you",
 "eks", "why", "zed", "open brace",
#else                             /* ALPHA_IN_DICT */
 "back quote", "A", "B", "C",
 "D", "E", "F", "G",
 "H", "I", "J", "K",
 "L", "M", "N", "O",
 "P", "Q", "R", "S",
 "T", "U", "V", "W",
 "X", "Y", "Z", "open brace",
#endif                            /* ALPHA_IN_DICT */
 "vertical bar", "close brace", "tilde", "delete",
 NULL
};
