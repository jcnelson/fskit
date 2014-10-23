/*
   fskit: a library for creating multi-threaded in-RAM filesystems
   Copyright (C) 2014  Jude Nelson

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
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