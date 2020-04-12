#include <config.h>
/* $Id: saynum.c,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
 */
char *saynum_id = "$Id: saynum.c,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $";

#include <stdio.h>
#include "proto.h"
#include "darray.h"

#include "rstruct.h"

#include "say.h"

/*
   **              Integer to Readable ASCII Conversion Routine.
   **
   ** Synopsis:
   **
   **      say_cardinal(value)
   **              long int     value;          -- The number to output
   **
   **      The number is translated into a string of words
   **
 */
static char *Cardinals[] =
{
 "zero", "one", "two", "three",
 "four", "five", "six", "seven",
 "eight", "nine",
 "ten", "eleven", "twelve", "thirteen",
 "fourteen", "fifteen", "sixteen", "seventeen",
 "eighteen", "nineteen"
};


static char *Twenties[] =
{
 "twenty", "thirty", "forty", "fifty",
 "sixty", "seventy", "eighty", "ninety"
};


static char *Ordinals[] =
{
 "zeroth", "first", "second", "third",
 "fourth", "fifth", "sixth", "seventh",
 "eighth", "ninth",
 "tenth", "eleventh", "twelfth", "thirteenth",
 "fourteenth", "fifteenth", "sixteenth", "seventeenth",
 "eighteenth", "nineteenth"
};


static char *Ord_twenties[] =
{
 "twentieth", "thirtieth", "fortieth", "fiftieth",
 "sixtieth", "seventieth", "eightieth", "ninetieth"
};

/*
   ** Translate a number to phonemes.  This version is for CARDINAL numbers.
   **       Note: this is recursive.
 */
unsigned
xlate_cardinal(synth,value, phone)
rsynth_synth *synth;
long int value;
darray_ptr phone;
{
 unsigned nph = 0;
 if (value < 0)
  {
   nph += xlate_string(synth,"minus", phone);
   value = (-value);
   if (value < 0)                 /* Overflow!  -32768 */
    {
     nph += xlate_string(synth, "a lot", phone);
     return nph;
    }
  }
 if (value >= 1000000000L)
  /* Billions */
  {
   nph += xlate_cardinal(synth, value / 1000000000L, phone);
   nph += xlate_string(synth, "billion", phone);
   value = value % 1000000000;
   if (value == 0)
    return nph;                   /* Even billion */
   if (value < 100)
    nph += xlate_string(synth, "and", phone);
   /* as in THREE BILLION AND FIVE */
  }
 if (value >= 1000000L)
  /* Millions */
  {
   nph += xlate_cardinal(synth, value / 1000000L, phone);
   nph += xlate_string(synth, "million", phone);
   value = value % 1000000L;
   if (value == 0)
    return nph;                   /* Even million */
   if (value < 100)
    nph += xlate_string(synth, "and", phone);
   /* as in THREE MILLION AND FIVE */
  }

 /* Thousands 1000..1099 2000..99999 */
 /* 1100 to 1999 is eleven-hunderd to ninteen-hunderd */
 if ((value >= 1000L && value <= 1099L) || value >= 2000L)
  {
   nph += xlate_cardinal(synth, value / 1000L, phone);
   nph += xlate_string(synth, "thousand", phone);
   value = value % 1000L;
   if (value == 0)
    return nph;                   /* Even thousand */
   if (value < 100)
    nph += xlate_string(synth, "and", phone);
   /* as in THREE THOUSAND AND FIVE */
  }
 if (value >= 100L)
  {
   nph += xlate_string(synth, Cardinals[value / 100], phone);
   nph += xlate_string(synth, "hundred", phone);
   value = value % 100;
   if (value == 0)
    return nph;                   /* Even hundred */
  }
 if (value >= 20)
  {
   nph += xlate_string(synth, Twenties[(value - 20) / 10], phone);
   value = value % 10;
   if (value == 0)
    return nph;                   /* Even ten */
  }
 nph += xlate_string(synth, Cardinals[value], phone);
 return nph;
}

/*
   ** Translate a number to phonemes.  This version is for ORDINAL numbers.
   **       Note: this is recursive.
 */
unsigned
xlate_ordinal(synth, value, phone)
rsynth_synth *synth;
long int value;
darray_ptr phone;
{
 unsigned nph = 0;
 if (value < 0)
  {
   nph += xlate_string(synth, "minus", phone);
   value = (-value);
   if (value < 0)                 /* Overflow!  -32768 */
    {
     nph += xlate_string(synth, "a lot", phone);
     return nph;
    }
  }
 if (value >= 1000000000L)
  /* Billions */
  {
   nph += xlate_cardinal(synth, value / 1000000000L, phone);
   value = value % 1000000000;
   if (value == 0)
    {
     nph += xlate_string(synth, "billionth", phone);
     return nph;                  /* Even billion */
    }
   nph += xlate_string(synth, "billion", phone);
   if (value < 100)
    nph += xlate_string(synth, "and", phone);
   /* as in THREE BILLION AND FIVE */
  }

 if (value >= 1000000L)
  /* Millions */
  {
   nph += xlate_cardinal(synth, value / 1000000L, phone);
   value = value % 1000000L;
   if (value == 0)
    {
     nph += xlate_string(synth, "millionth", phone);
     return nph;                  /* Even million */
    }
   nph += xlate_string(synth, "million", phone);
   if (value < 100)
    nph += xlate_string(synth, "and", phone);
   /* as in THREE MILLION AND FIVE */
  }

 /* Thousands 1000..1099 2000..99999 */
 /* 1100 to 1999 is eleven-hunderd to ninteen-hunderd */
 if ((value >= 1000L && value <= 1099L) || value >= 2000L)
  {
   nph += xlate_cardinal(synth, value / 1000L, phone);
   value = value % 1000L;
   if (value == 0)
    {
     nph += xlate_string(synth, "thousandth", phone);
     return nph;                  /* Even thousand */
    }
   nph += xlate_string(synth, "thousand", phone);
   if (value < 100)
    nph += xlate_string(synth, "and", phone);
   /* as in THREE THOUSAND AND FIVE */
  }
 if (value >= 100L)
  {
   nph += xlate_string(synth, Cardinals[value / 100], phone);
   value = value % 100;
   if (value == 0)
    {
     nph += xlate_string(synth, "hundredth", phone);
     return nph;                  /* Even hundred */
    }
   nph += xlate_string(synth, "hundred", phone);
  }
 if (value >= 20)
  {
   if ((value % 10) == 0)
    {
     nph += xlate_string(synth, Ord_twenties[(value - 20) / 10], phone);
     return nph;                  /* Even ten */
    }
   nph += xlate_string(synth, Twenties[(value - 20) / 10], phone);
   value = value % 10;
  }
 nph += xlate_string(synth, Ordinals[value], phone);
 return nph;
}
