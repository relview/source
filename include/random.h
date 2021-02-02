#ifndef _RANDOM_H
#  define _RANDOM_H

#include "config.h"

#ifdef USE_SFMT
#  include "SFMT.h"
#else
#  include "limits.h"
#  include "stdlib.h"
#endif

/*! wrapper function for either the libc random number 'srandom'
 * function, or the corresponding SFMT function. 
 * 
 * @param seed The seed.
 */
static void _srandom (unsigned int seed)
{
#ifdef USE_SFMT
  init_gen_rand (seed);
#else
  srandom (seed);
#endif
}

/*! wrapper function for either the libc 'random' function
 * function, or the corresponding SFMT function. 
 * 
 * @return Returns a floating number in [0,1].
 */
static float _randomf ()
{
#ifdef USE_SFMT
  return (float) genrand_real1 ();
#else
  /* RAND_MAX doesn't work on SunOS. */
  return (float)(random () / (double) INT_MAX);
#endif
}

#endif
