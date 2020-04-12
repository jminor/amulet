#include <config.h>
/* $Id: say.c,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
   $Log: say.c,v $
   Revision 1.1.1.1  2002/05/15 00:36:40  moocow
   # release 2.1.3 -- new directory structure

   Revision 1.3  2002/05/11 22:32:01  moocow
   + extracted globals from holmes.c

   Revision 1.2  2002/05/11 21:14:20  moocow
   + added audio file information and klatt synth to 'rsynth_synth'
     structure; removed globals

   Revision 1.1.1.1  2002/05/09 11:52:51  moocow
   + this is, and i mean it, the initial cvs import



 * Branch "moocow" Tue May  7 18:02:35 CEST 2002
 *   - libtool-ized by moocow <moocow@ling.uni-potsdam.de>
 *
 * Revision 1.13  1994/11/08  13:30:50  a904209
 * 2.0 release
 *
 * Revision 1.12  1994/11/04  13:32:31  a904209
 * 1.99.1 - Change configure stuff
 *
 * Revision 1.11  1994/11/02  10:55:31  a904209
 * Add autoconf. Tested on SunOS/Solaris
 *
 * Revision 1.10  1994/10/04  17:12:50  a904209
 * 3rd pre-release
 *
 * Revision 1.9  1994/10/04  09:08:27  a904209
 * Next Patch merge
 *
 * Revision 1.8  1994/10/03  08:41:47  a904209
 * 2nd pre-release                                                         
 *
 * Revision 1.7  1994/09/19  15:48:29  a904209
 * Split hplay.c, gdbm dictionary, start of f0 contour, netaudio and HP ports
 *
 * Revision 1.6  1994/04/15  16:47:37  a904209
 * Edits for Solaris2.3 (aka SunOs 5.3)
 *
 * Revision 1.5  1994/02/24  15:03:05  a904209
 * Added contributed linux, NeXT and SGI ports.
 *
 * Revision 1.4  93/11/18  16:29:06  a904209
 * Migrated nsyth.c towards Jon's scheme - merge still incomplete
 * 
 * Revision 1.3  93/11/16  14:32:44  a904209
 * Added RCS Ids, partial merge of Jon's new klatt/parwave
 *
 * Revision 1.3  93/11/16  14:00:58  a904209
 * Add IDs and merge Jon's klatt sources - incomplete
 *
 */

extern char *Revision;

/* shared with 'main.c' */
#include <stdio.h>
#include <ctype.h>
#include <useconfig.h>
#include <math.h>
#include "proto.h"

#include "rstruct.h"

#include "nsynth.h"
#include "hplay.h"
#include "dict.h"
#include "ASCII.h"
#include "darray.h"
#include "holmes.h"
#include "phtoelm.h"
#include "getargs.h"
#include "text.h"
#include "phones.h"
#include "file.h"
#include "say.h"


/*static int verbose = 0;*/

unsigned
spell_out(synth, word, n, phone)
rsynth_synth *synth;
char *word;
int n;
darray_ptr phone;
{
 unsigned nph = 0;
 fprintf(stderr, "Spelling '%.*s'\n", n, word);
 while (n-- > 0)
  {
   nph += xlate_string(synth, ASCII[*word++ & 0x7F], phone);
  }
 return nph;
}

int
suspect_word(synth, s, n)
rsynth_synth *synth;
char *s;
int n;
{
 int i = 0;
 int seen_lower = 0;
 int seen_upper = 0;
 int seen_vowel = 0;
 int last = 0;
 for (i = 0; i < n; i++)
  {
   char ch = *s++;
   if (i && last != '-' && isupper(ch))
    seen_upper = 1;
   if (islower(ch))
    {
     seen_lower = 1;
     ch = toupper(ch);
    }
   if (ch == 'A' || ch == 'E' || ch == 'I' || ch == 'O' || ch == 'U' || ch == 'Y')
    seen_vowel = 1;
   last = ch;
  }
 return !seen_vowel || (seen_upper && seen_lower) || !seen_lower;
}

static unsigned xlate_word PROTO((rsynth_synth *synth, char *word, int n, darray_ptr phone));

static unsigned
xlate_word(synth, word, n, phone)
rsynth_synth *synth;
char *word;
int n;
darray_ptr phone;
{
 unsigned nph = 0;
 if (*word != '[')
  {
   if (synth->rsdict.dict)
    {
     unsigned char *p = dict_find(&(synth->rsdict), word, n);
     if (p)
      {
       unsigned char *s = p;
       while (*s)
        {
         char *x = synth->rsdict.dialect[(unsigned) (*s++)];
         while (*x)
          {
           phone_append(phone, *x++);
           nph++;
          }
        }
       phone_append(phone, ' ');
       free(p);
       return nph + 1;
      }
     else
      {
       /* If supposed word contains '.' or '-' try breaking it up... */
       char *h = word;
       while (h < word + n)
        {
         if (*h == '.' || *h == '-')
          {
           nph += xlate_word(synth, word, h++ - word, phone);
           nph += xlate_word(synth, h, word + n - h, phone);
           return nph;
          }
         else
          h++;
        }
      }
    }
   if (suspect_word(synth, word, n))
    return spell_out(synth, word, n, phone);
   else
    {
     if (synth->rsdict.dict || synth->verbose)
      fprintf(stderr, "Guess %p '%.*s'\n", synth->rsdict.dict, n, word);
     nph += NRL(word, n, phone);
    }
  }
 else
  {
   if ((++word)[(--n) - 1] == ']')
    n--;
   while (n-- > 0)
    {
     phone_append(phone, *word++);
     nph++;
    }
  }
 phone_append(phone, ' ');
 return nph + 1;
}

void
say_phones(synth, phone, len, verbose)
rsynth_synth *synth;
char *phone;
int len;
int verbose;
{
 darray_t elm;
 unsigned frames;
 darray_init(&elm, sizeof(char), len);
 if ((frames = phone_to_elm(synth, phone, len, &elm)))
  {
   unsigned max_samples = frames * synth->klatt_global.nspfr;
   short *samp = (short *) malloc(sizeof(short) * max_samples);
   if (verbose)
    fprintf(stderr,"%.*s\n", len, phone);
   if (samp)
    {
     unsigned nsamp = holmes(synth, elm.items, (unsigned char *) darray_find(&elm, 0),
                             max_samples, samp);
     audio_play(synth, nsamp, samp);
     if (synth->file_write) {
       (*(file_write_p)synth->file_write)(synth, nsamp, samp);
     }
     free(samp);
    }
  }
 darray_free(&elm);
}




unsigned
xlate_string(synth, string, phone)
rsynth_synth *synth;
char *string;
darray_ptr phone;
{
 unsigned nph = 0;
 char *s = string;
 char ch;
 while (isspace(ch = *s))
  s++;
 while ((ch = *s))
  {
   char *word = s;
   if (isalpha(ch))
    {
     while (isalpha(ch = *s) || ((ch == '\'' || ch == '-' || ch == '.') && isalpha(s[1])))
      s++;
     if (!ch || isspace(ch) || ispunct(ch) || (isdigit(ch) && !suspect_word(synth, word, s - word)))
      nph += xlate_word(synth, word, s - word, phone);
     else
      {
       while ((ch = *s) && !isspace(ch) && !ispunct(ch))
        s++;
       nph += spell_out(synth, word, s - word, phone);
      }
    }
   else if (isdigit(ch) || (ch == '-' && isdigit(s[1])))
    {
     int sign = (ch == '-') ? -1 : 1;
     long value = 0;
     if (sign < 0)
      ch = *++s;
     while (isdigit(ch = *s))
      {
       value = value * 10 + ch - '0';
       s++;
      }
     if (ch == '.' && isdigit(s[1]))
      {
       word = ++s;
       nph += xlate_cardinal(synth, value * sign, phone);
       nph += xlate_string(synth, "point", phone);
       while (isdigit(ch = *s))
        s++;
       nph += spell_out(synth, word, s - word, phone);
      }
     else
      {
       /* check for ordinals, date, time etc. can go in here */
       nph += xlate_cardinal(synth, value * sign, phone);
      }
    }
   else if (ch == '[' && strchr(s, ']'))
    {
     char *word = s;
     while (*s && *s++ != ']')
      /* nothing */ ;
     nph += xlate_word(synth, word, s - word, phone);
    }
   else if (ispunct(ch))
    {
     switch (ch)
      {
        /* On end of sentence flush the buffer ... */
       case '!':
       case '?':
       case '.':
        if ((!s[1] || isspace(s[1])) && phone->items)
         {
          say_phones(synth, (char *) darray_find(phone, 0), phone->items, synth->verbose);
          phone->items = 0;
         }
        s++;
        phone_append(phone, ' ');
        break;
       case '"':                 /* change pitch ? */
       case ':':
       case '-':
       case ';':
       case ',':
       case '(':
       case ')':
        s++;
        phone_append(phone, ' ');
        break;
       case '[':
        {
         char *e = strchr(s, ']');
         if (e)
          {
           s++;
           while (s < e)
            phone_append(phone, *s++);
           s = e + 1;
           break;
          }
        }
       default:
        nph += spell_out(synth, word, 1, phone);
        s++;
        break;
      }
    }
   else
    {
     while ((ch = *s) && !isspace(ch))
      s++;
     nph += spell_out(synth, word, s - word, phone);
    }
   while (isspace(ch = *s))
    s++;
  }
 return nph;
}

char *
concat_args(argc, argv)
int argc;
char *argv[];
{
 int len = 0;
 int i;
 char *buf;
 for (i = 1; i < argc; i++)
  len += strlen(argv[i]) + 1;
 buf = (char *) malloc(len);
 if (buf)
  {
   char *d = buf;
   for (i = 1; i < argc;)
    {
     char *s = argv[i++];
     while (*s)
      *d++ = *s++;
     if (i < argc)
      *d++ = ' ';
     else
      *d = '\0';
    }
  }
 return buf;
}

void
say_string(synth,s)
rsynth_synth *synth;
char *s;
{
 darray_t phone;
 darray_init(&phone, sizeof(char), 128);
 xlate_string(synth, s, &phone);
 if (phone.items)
  say_phones(synth, (char *) darray_find(&phone, 0), phone.items, synth->verbose);
 darray_free(&phone);
}

extern int darray_fget PROTO((FILE * f, darray_ptr p));

int
darray_fget(f, p)
FILE *f;
darray_ptr p;
{
 int ch;
 while ((ch = fgetc(f)) != EOF)
  {
   phone_append(p, ch);
   if (ch == '\n')
    break;
  }
 phone_append(p, '\0');
 return p->items - 1;
}

extern void say_file PROTO((rsynth_synth *synth,FILE * f));

void
say_file(synth,f)
rsynth_synth *synth;
FILE *f;
{
 darray_t line;
 darray_t phone;
 darray_init(&line, sizeof(char), 128);
 darray_init(&phone, sizeof(char), 128);
 while (darray_fget(f, &line))
  {
   xlate_string(synth, (char *) darray_find(&line, 0), &phone);
   line.items = 0;
  }
 if (phone.items)
  say_phones(synth, (char *) darray_find(&phone, 0), phone.items, synth->verbose);
 darray_free(&phone);
 darray_free(&line);
}

/* main only */
/*int main PROTO((int argc, char *argv[], char *env[]));*/
