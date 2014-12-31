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

#ifndef _FSKIT_DEBUG_H_
#define _FSKIT_DEBUG_H_

#include "common.h"

#define FSKIT_WHERESTR "%05d:%05d: [%16s:%04u] %s: "
#define FSKIT_WHEREARG (int)getpid(), (int)pthread_self(), __FILE__, __LINE__, __func__

#define fskit_debug( format, ... ) \
   do { \
      if( FSKIT_GLOBAL_DEBUG_MESSAGES ) { \
         printf( FSKIT_WHERESTR format, FSKIT_WHEREARG, __VA_ARGS__ ); \
         fflush(stdout); \
      } \
   } while(0)


#define fskit_error( format, ... ) \
   do { \
      if( FSKIT_GLOBAL_ERROR_MESSAGES ) { \
         fprintf(stderr, FSKIT_WHERESTR format, FSKIT_WHEREARG, __VA_ARGS__); \
         fflush(stderr); \
      } \
   } while(0)


extern "C" {

extern int FSKIT_GLOBAL_DEBUG_LOCKS;
extern int FSKIT_GLOBAL_DEBUG_MESSAGES;
extern int FSKIT_GLOBAL_ERROR_MESSAGES;

void fskit_set_debug_level( int d );
void fskit_set_error_level( int e );
int fskit_get_debug_level();
int fskit_get_error_level();

}

#endif
