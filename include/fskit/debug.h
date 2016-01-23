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

#include <fskit/common.h>

#define FSKIT_WHERESTR "%05d:%016llX: [fskit %16s:%04u] %s %s: "
#define FSKIT_WHEREARG (int)getpid(), fskit_pthread_self(), __FILE__, __LINE__, __func__

#define fskit_debug( format, ... ) \
   do { \
      if( FSKIT_GLOBAL_DEBUG_MESSAGES ) { \
         fprintf(stderr, FSKIT_WHERESTR format, FSKIT_WHEREARG, "DEBUG", __VA_ARGS__ ); \
      } \
   } while(0)


#define fskit_error( format, ... ) \
   do { \
      if( FSKIT_GLOBAL_ERROR_MESSAGES ) { \
         fprintf(stderr, FSKIT_WHERESTR format, FSKIT_WHEREARG, "ERROR", __VA_ARGS__); \
      } \
   } while(0)


FSKIT_C_LINKAGE_BEGIN 

extern int FSKIT_GLOBAL_DEBUG_LOCKS;
extern int FSKIT_GLOBAL_DEBUG_MESSAGES;
extern int FSKIT_GLOBAL_ERROR_MESSAGES;

void fskit_set_debug_level( int d );
void fskit_set_error_level( int e );
int fskit_get_debug_level();
int fskit_get_error_level();

// portable cast pthread_t to uint64_t 
unsigned long long int fskit_pthread_self(void);

FSKIT_C_LINKAGE_END 

#endif
