/* $Id: proto.h,v 1.1.1.1 2002/05/15 00:36:40 moocow Exp $
*/
#ifndef PROTO
#if defined (USE_PROTOTYPES) ? USE_PROTOTYPES : defined (__STDC__)
#define PROTO(ARGS) ARGS
#ifndef NOPROTOFLOAT
#define Float float
#else
#define Float double
#endif
#else
#define PROTO(ARGS) ()
#define Float double
#endif

#ifndef __GNUC__
#define inline
#endif

#endif /* PROTO */
