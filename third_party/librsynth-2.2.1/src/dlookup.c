#include <config.h>
#include "proto.h"
#include <stdio.h>
#include <ctype.h>
#include <useconfig.h>

#include "rstruct.h"

#include "dict.h"
#include "phones.h"
#include "getargs.h"

extern char *ph_am[];
rsynth_dict rsdict =
  {
    NULL,  /* void *dict */
    ph_am, /* char **dialect */
    "b",   /* char *dict_path */
  };


static void show PROTO((char *s));
static void 
show(s)
char *s;
{
 unsigned char *p = dict_find(&rsdict, s, strlen(s));
 printf("%s", s);
 if (p)
  {
   int l = strlen((char *)p);
   int i;
   for (i = 0; i < l; i++)
    printf(" %s", ph_name[(unsigned) (p[i])]);
   printf(" [");
   for (i = 0; i < l; i++)
    printf("%s", rsdict.dialect[(unsigned) (p[i])]);
   printf("]\n");
   free(p);
  }
 else
  printf(" ???\n");
}

int help_only = 0;

int main PROTO((int argc, char *argv[], char *env[]));

int
main(argc, argv, envp)
int argc;
char *argv[];
char *envp[];
{
 argc = rsdict_init(&rsdict, &help_only, argc, argv);
 if (help_only)
  {
   fprintf(stderr,"Usage: %s [options as above] words to lookup\n",argv[0]);
  }
 else
  {
   if (rsdict.dict)        
    {               
     int i;
     for (i=1; i < argc; i++)
      {
       show(argv[i]);
      }
     dict_term(&rsdict);
    }               
  }
 return 0;
}
