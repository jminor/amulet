#include <config.h>
#include "proto.h"

#include <stdio.h>
#include <fcntl.h>
#include <useconfig.h>

#include "rstruct.h"

#include "getargs.h"
#include "l2u.h"
#include "hplay.h"
#include "file.h"

/* for synchronization */
#include <unistd.h>
#include <errno.h>


static void wblong PROTO((int fd, unsigned long x));
static void 
wblong(fd, x)
int fd;
unsigned long x;
{
 int i;
 for (i = 24; i >= 0; i -= 8)
  {
   char byte = (char) ((x >> i) & 0xFF);
   write(fd, &byte, 1);
  }
}

extern void au_header PROTO((int fd,unsigned enc,unsigned rate,unsigned size,char *comment));

void 
au_header(fd, enc, rate, size, comment)
int fd;
unsigned enc;
unsigned rate;
unsigned size;
char *comment;
{
 if (!comment)
  comment = "";
 wblong(fd, SUN_MAGIC);
 wblong(fd, SUN_HDRSIZE + strlen(comment));
 wblong(fd, size);
 wblong(fd, enc);
 wblong(fd, rate);
 wblong(fd, 1);                   /* channels */
 write(fd, comment, strlen(comment));
}

static void aufile_write PROTO((rsynth_synth *synth,int n,short *data));

static void
aufile_write(synth, n, data)
rsynth_synth *synth;
int n;
short *data;
{
 if (n > 0)
  {
   if (synth->linear_fd >= 0)
    {
     unsigned size = n * sizeof(short);
     if (write(synth->linear_fd, data, n * sizeof(short)) != size)
            perror("librsynth: error writing to raw linear file");
    }
   if (synth->au_fd >= 0)
    {
     if (synth->au_encoding == SUN_LIN_16)
      {
       unsigned size = n * sizeof(short);
       if (write(synth->au_fd, data, size) != size)
        perror("librsynth: wrror writing to au file");
       else
        synth->au_size += size;
      }
     else if (synth->au_encoding == SUN_ULAW)
      {
       unsigned char *plabuf = (unsigned char *) malloc(n);
       if (plabuf)
        {
         unsigned char *p = plabuf;
         unsigned char *e = p + n;
         while (p < e)
          {
           *p++ = short2ulaw(*data++);
          }
         if (write(synth->au_fd, plabuf, n) != n)
          perror("librsynth: error writing to au file");
         else
          synth->au_size += n;
         free(plabuf);
        }
       else
        {
         fprintf(stderr, "librsynth: No memory for ulaw data\n");
        }
      }
     else
      {
       abort();
      }
    }
  }
}

/*------------------------------------------------------------------------
 * sync
 *------------------------------------------------------------------------*/
int rsynth_sync_file PROTO((rsynth_synth *synth));

int
rsynth_sync_file(synth)
     rsynth_synth *synth;
{

  //-- Sync ulaw file
  if (synth->au_fd >= 0) {
    //-- truncate
    ftruncate(synth->au_fd, lseek(synth->au_fd, 0L, SEEK_CUR));
    //-- and sync
    if (fsync(synth->au_fd) < 0) {
      perror("librsynth: error synchronzing au file");
    }
  }

  //-- Sync linear file
  if (synth->linear_fd >= 0) {
    //-- truncate
    ftruncate(synth->linear_fd, lseek(synth->linear_fd, 0L, SEEK_CUR));
    //-- and sync
    if (fsync(synth->linear_fd) < 0) {
      perror("librsynth: error synchronzing linear file");
    }
  }

  return 1;
}


/*------------------------------------------------------------------------
 * stop & term
 *------------------------------------------------------------------------*/
static void aufile_term PROTO((rsynth_synth *synth));

static void
aufile_term(synth)
     rsynth_synth *synth;
{
  //-- stop synth: close files
  rsynth_stop_file(synth);
}

int rsynth_stop_file(synth)
     rsynth_synth *synth;
{
  int rc;

  // Sync files
  rc = rsynth_sync_file(synth);

  // Close ulaw file
  if (synth->au_fd >= 0) {
    off_t here = lseek(synth->au_fd, 0L, SEEK_CUR);
    if (here >= 0) {
      //-- can seek this file - truncate it
      ftruncate(synth->au_fd, here);
      //-- Now go back and overwite header with actual size
      if (lseek(synth->au_fd, 8L, SEEK_SET) == 8) {
	wblong(synth->au_fd, synth->au_size);
      }
      //-- now go back to the end of the file
      lseek(synth->au_fd, here, SEEK_SET);
   }
    // close it
    close(synth->au_fd);
    synth->au_fd = -1;
  }

  // Close linear file
  if (synth->linear_fd >= 0) close(synth->linear_fd);
  synth->linear_fd = -1;

 return 1;
}


/*------------------------------------------------------------------------
 * init & start
 *------------------------------------------------------------------------*/
int
file_init(synth, argc, argv)
     rsynth_synth *synth;
     int argc;
     char *argv[];
{
 argc = getargs(&(synth->help_only),
		"File output", argc, argv,
                "l", "", &(synth->linear_filename), "Raw 16-bit linear pathname",
                "o", "", &(synth->au_filename),     "Sun/Next audio file name",
                NULL);
 if (synth->help_only)
  return argc;

 rsynth_start_file(synth);
 return argc;
}


int rsynth_start_file(synth)
  rsynth_synth *synth;
 {
   // au_file
   if (synth->au_filename) {

     if (strcmp(synth->au_filename, "-") == 0) {
       synth->au_fd = 1;                   /* stdout */
     } else {
       synth->au_fd = open(synth->au_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
       if (synth->au_fd < 0) {
	 fprintf(stderr, "librsynth: open failed for au file '%s': %s",
		 synth->au_filename, strerror(errno));
	 return 0;
       }
     }

     if (synth->au_fd >= 0) {
       if (synth->samp_rate > 8000) {
	 synth->au_encoding = SUN_LIN_16;
       } else {
	 synth->au_encoding = SUN_ULAW;
       }
       au_header(synth->au_fd, synth->au_encoding, synth->samp_rate, SUN_UNSPEC, "");
       synth->au_size = 0;
     }
   }

   // linear file
   if (synth->linear_filename) {
     if (strcmp(synth->linear_filename, "-") == 0) {
       synth->linear_fd = 1 /* stdout */ ;
     } else {
       synth->linear_fd = open(synth->linear_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
     }
     
     if (synth->linear_fd < 0) {
	 fprintf(stderr, "librsynth: open failed for linear file '%s': %s",
		 synth->linear_filename, strerror(errno));
       return 0;
     }
   }

   // handlers
   if (synth->au_fd >= 0 || synth->linear_fd >= 0) {
     synth->file_write = aufile_write;
     synth->file_term  = aufile_term;
   }

   return 1;
}

