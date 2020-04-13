#include <config.h>
#include "proto.h"
#include <stdio.h>
#include "phones.h"

//--now in rstruct.h
//void *dict;
//char **dialect = ph_am;


#include "rstruct.h"

#ifdef HAVE_LIBGDBM
#include <useconfig.h>
#include <ctype.h>
#include <gdbm.h>

#include "dict.h"
#include "getargs.h"


#ifndef DICT_DIR
#define DICT_DIR "/usr/local/lib"
#endif

//--now in rstruct.h
//char *dict_path = "b";

char *default_dict_name = "b";


unsigned char *
dict_find(rsdict, s, n)
rsynth_dict *rsdict;
char *s;
unsigned n;
{
 if (!n)
  n = strlen(s);
 if (rsdict->dict)
  {
   datum key;
   datum data;
   int i;
   key.dptr = malloc(n);
   key.dsize = n;
   for (i = 0; i < n; i++)
    {
     key.dptr[i] = (islower(s[i])) ? toupper(s[i]) : s[i];
    }
   data = gdbm_fetch((GDBM_FILE) rsdict->dict, key);
   free(key.dptr);
   if (data.dptr)
    {
     unsigned char *w = (unsigned char *) malloc(data.dsize + 1);
     memcpy(w, data.dptr, data.dsize);
     w[data.dsize] = 0;
     free(data.dptr);
     return w;
    }
  }
 return NULL;
}

static void choose_dialect PROTO((rsynth_dict *rsdict));

static void
choose_dialect(rsdict)
rsynth_dict *rsdict;
{
 unsigned char *word = dict_find(rsdict, "schedule", 0);
 if (word)
  {
   if (word[0] == SH)
    rsdict->dialect = ph_br;
   else if (word[0] == S && word[1] == K)
    rsdict->dialect = ph_am;
   free(word);
  }
}


/*------------------------------------------------------------------------
 * init & start
 *------------------------------------------------------------------------*/
int rsdict_start_dict(rsdict)
     rsynth_dict *rsdict;
{

  /*
   * Argh: not for gdbm-1.7.3 (debian woody,sarge)
   */
   //if (rsdict->dict && gdbm_fdesc(rsdict->dict)) {
   if (rsdict && rsdict->dict) {
    // dictionary is already open -- skip it
    return 1;
  }

  if (!rsdict->dict_path) {
    // no dictionary specified -- take it gracefully
#  ifdef RSYNTH_DICT_WARN_VERBOSE
    fprintf(stderr, "librsynth warning: no dictionary specified.\n");
#  endif
    return 1;
  }
  /*if (!access(rsdict->dict_path,R_OK)) {
    // no dictionary present -- mecker
    fprintf(stderr, "librsynth warning: cannot access dictionary file `%s'.\n", rsdict->dict_path);
    return 1;
    }*/

  // back to the grind
  if (!(rsdict->dict = gdbm_open(rsdict->dict_path, GDBM_READER, 0, 0666, NULL))) {
    fprintf(stderr, "librsynth error: failed to open dictionary file `%s': ", rsdict->dict_path);
    perror(gdbm_strerror(gdbm_errno));
    return 0;
  }

  if (rsdict->dict){
    choose_dialect(rsdict);
  } else {
    fprintf(stderr, "librsynth error: no GDBM_FILE pointer in rsdict_start_dict().\n");
    return 0;
  }

  return 1;
}

int
rsdict_init(rsdict, help_only, argc, argv)
rsynth_dict *rsdict;
int *help_only;
int argc;
char *argv[];
{
  char *dict_name = NULL;

  argc = getargs(help_only,
		 "Dictionary", argc, argv,
		 "d", "", &dict_name, "Which dictionary [b|a]",
		 NULL);

 if (*help_only)
   return argc;

  // compatibility hack
  if (dict_name) {
    // wipe old dict_path if necessary
    if (rsdict->dict_path) {
      free(rsdict->dict_path);
    }
    // hack for single-letter dictionary-names
    if (strlen(dict_name) == 1) {
      rsdict->dict_path = malloc(strlen(DICT_DIR) + strlen("Dict.db") + strlen(dict_name) + 2);
      sprintf(rsdict->dict_path, "%s/%sDict.db", DICT_DIR, dict_name);
      free(dict_name);
    } else {
      rsdict->dict_path = dict_name;
    }
  } else {
    // default dictionary
    rsdict->dict_path = malloc(strlen(DICT_DIR) + strlen("Dict.db") + 3);
    sprintf(rsdict->dict_path, "%s/%sDict.db", DICT_DIR, default_dict_name);
  }

 rsdict_start_dict(rsdict);
 return argc;
}


/*------------------------------------------------------------------------
 * stop & term
 *------------------------------------------------------------------------*/
int rsdict_stop_dict(rsdict)
     rsynth_dict *rsdict;
{
  if (rsdict->dict) {
    gdbm_close((GDBM_FILE) rsdict->dict);
    rsdict->dict = NULL;
  }
  return 1;
}

int rsynth_stop_dict(synth)
rsynth_synth *synth;
{
  return rsdict_stop_dict(&(synth->rsdict));
}


void dict_term(rsdict)
rsynth_dict *rsdict;
{
 if (rsdict->dict)
  gdbm_close((GDBM_FILE) rsdict->dict);
}

/*------------------------------------------------------------------------
 * wrappers
 *------------------------------------------------------------------------*/
int
dict_init(synth, argc, argv)
rsynth_synth *synth;
int argc;
char *argv[];
{
  return rsdict_init(&(synth->rsdict), &(synth->help_only), argc, argv);
}

int rsynth_start_dict(synth)
     rsynth_synth *synth;
{
  return rsdict_start_dict(&(synth->rsdict));
}

#else

unsigned char * dict_find(rsdict, s, n)
rsynth_dict *rsdict;
char *s;
unsigned n;
{
 return NULL;
}

int dict_init(rsdict, argc, argv)
rsynth_dict *rsdict;
int argc;
char *argv[];
{
 return argc;
}

void dict_term(rsdict)
rsynth_dict *rsdict;
{
}

int rsynth_start_dict(synth)
     rsynth_synth *synth;
{
  return 1;
}

int rsynth_stop_dict(synth)
rsynth_synth *synth;
{
  return 1;
}

#endif
