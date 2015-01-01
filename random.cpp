/*
   fskit: a library for creating multi-threaded in-RAM filesystems
   Copyright (C) 2014  Jude Nelson

   This program is dual-licensed: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License version 3 or later as
   published by the Free Software Foundation. For the terms of this
   license, see LICENSE.LGPLv3+ or <http://www.gnu.org/licenses/>.

   You are free to use this program under the terms of the GNU Lesser General
   Public License, but WITHOUT ANY WARRANTY; without even the implied
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   See the GNU Lesser General Public License for more details.

   Alternatively, you are free to use this program under the terms of the
   Internet Software Consortium License, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
   For the terms of this license, see LICENSE.ISC or
   <http://www.isc.org/downloads/software-support-policy/isc-license/>.
*/

#include "random.h"

static uint32_t Q[4096], c=362436;     /* choose random initial c<809430660 and */
                                       /* 4096 random 32-bit integers for Q[]   */

static pthread_mutex_t CMWC4096_lock = PTHREAD_MUTEX_INITIALIZER;

// initialize random state
int fskit_random_init() {

   int rfd = open(FSKIT_RANDOM_DEVICE_PATH, O_RDONLY );
   if( rfd < 0 ) {
      return -errno;
   }

   ssize_t nr = read( rfd, Q, 4096 * sizeof(uint32_t) );
   if( nr < 0 ) {
      return -errno;
   }
   if( nr != 4096 * sizeof(uint32_t) ) {
      close( rfd );
      return -ENODATA;
   }

   close( rfd );
   return 0;
}

// Mersenne Twister for unsigned 32-bit numbers
uint32_t fskit_random32() {

   pthread_mutex_lock( &CMWC4096_lock );

   uint64_t t, a=18782LL;
   static uint32_t i=4095;
   uint32_t x,r=0xfffffffe;

   i=(i+1)&4095;
   t=a*Q[i]+c;
   c=(t>>32);
   x=t+c;

   if( x < c ) {
      x++;
      c++;
   }

   Q[i]=r-x;

   uint32_t ret = Q[i];

   pthread_mutex_unlock( &CMWC4096_lock );
   return ret;
}
