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

#include "utime.h"


// set access time to the given time as "now"
// if now is NULL, get the time from CLOCK_REALTIME and use that for "now"
// fent must be write-locked, or unable to be written by any other thread
int fskit_entry_set_ctime( struct fskit_entry* fent, struct timespec* now ) {
   
   struct timespec new_time;
   
   if( now == NULL ) {
      int rc = clock_gettime( CLOCK_REALTIME, &new_time );
      if( rc != 0 ) {
         rc = -errno;
         errorf("clock_gettime rc = %d\n", rc);
         return rc;
      }
   }
   else {
      memcpy( &new_time, now, sizeof(struct timespec) );
   }
   
   fent->ctime_sec = new_time.tv_sec;
   fent->ctime_nsec = new_time.tv_nsec;
   
   return 0;
}

// set modification time to the given time as "now"
// if now is NULL, get the time from CLOCK_REALTIME and use that for "now"
// fent must be write-locked, or unable to be written by any other thread
int fskit_entry_set_mtime( struct fskit_entry* fent, struct timespec* now ) {
   
   struct timespec new_time;
   
   if( now == NULL ) {
      int rc = clock_gettime( CLOCK_REALTIME, &new_time );
      if( rc != 0 ) {
         rc = -errno;
         errorf("clock_gettime rc = %d\n", rc);
         return rc;
      }
   }
   else {
      memcpy( &new_time, now, sizeof(struct timespec) );
   }
   
   fent->mtime_sec = new_time.tv_sec;
   fent->mtime_nsec = new_time.tv_nsec;
   
   return 0;
}


// set access time to the given time as "now"
// if now is NULL, get the time from CLOCK_REALTIME and use that for "now"
// fent must be write-locked, or unable to be written by any other thread
int fskit_entry_set_atime( struct fskit_entry* fent, struct timespec* now ) {
   
   struct timespec new_time;
   
   if( now == NULL ) {
      int rc = clock_gettime( CLOCK_REALTIME, &new_time );
      if( rc != 0 ) {
         rc = -errno;
         errorf("clock_gettime rc = %d\n", rc);
         return rc;
      }
   }
   else {
      memcpy( &new_time, now, sizeof(struct timespec) );
   }
   
   fent->atime_sec = new_time.tv_sec;
   fent->atime_nsec = new_time.tv_nsec;
   
   return 0;
}
