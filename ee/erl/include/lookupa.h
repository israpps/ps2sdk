/*
------------------------------------------------------------------------------
By Bob Jenkins, September 1996.
lookupa.h, a hash function for table lookup, same function as lookup.c.
Use this code in any way you wish.  Public Domain.  It has no warranty.
Source is http://burtleburtle.net/bob/c/lookupa.h
------------------------------------------------------------------------------
*/

#ifndef STANDARD
#include "standard.h"
#endif

#ifndef LOOKUPA
#define LOOKUPA

#ifdef __cplusplus
extern "C" {
#endif

#define CHECKSTATE 8
#define hashsize(n) ((ub4)1<<(n))
#define hashmask(n) (hashsize(n)-1)

extern ub4  lookup(/*_ ub1 *k, ub4 length, ub4 level _*/);
extern void checksum(/*_ ub1 *k, ub4 length, ub4 *state _*/);

#ifdef __cplusplus
}
#endif

#endif /* LOOKUPA */
