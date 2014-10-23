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
#include "path.h"
#include "entry.h"

// set access time to the given time as "now"
// if now is NULL, get the time from CLOCK_REALTIME and use that for "now"
// fent must be write-locked, or unable to be written by any other thread
int fskit_entry_set_ctime( struct fskit_entry* fent, struct timespec* now ) {
   
   struct timespec new_time;
   
   if( now == NULL ) {
      int rc = clock_gettime( CLOCK_REALTIME, &new_time );
      if( rc != 0 ) {
         rc = -errno;
         fskit_error("clock_gettime rc = %d\n", rc);
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
         fskit_error("clock_gettime rc = %d\n", rc);
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
         fskit_error("clock_gettime rc = %d\n", rc);
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

// set access and modtime the POSIX way 
// return 0 on success, and the usual error methods for path resolution on error 
int fskit_utime( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, const struct utimbuf* times ) {
   
   struct timeval utimes[2];
   
   utimes[0].tv_sec = times->actime;
   utimes[0].tv_usec = 0;
   
   utimes[1].tv_sec = times->modtime;
   utimes[1].tv_usec = 0;
   
   return fskit_utimes( core, path, user, group, utimes );
}

// set access and modtime in the POSIX way 
// return 0 on success, and the usual error methods for a path resolution error
int fskit_utimes( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, const struct timeval times[2] ) {
   
   int rc = 0;
   
   struct fskit_entry* fent = fskit_entry_resolve_path( core, path, user, group, true, &rc );
   if( fent == NULL ) {
      
      return rc;
   }
   
   // need write access
   if( !FSKIT_ENTRY_IS_WRITEABLE( fent->mode, fent->owner, fent->group, user, group ) ) {
      
      fskit_entry_unlock( fent );
      return -EACCES;
   }
   
   struct timeval atime;
   struct timeval mtime;
   
   memset( &atime, 0, sizeof(struct timeval) );
   memset( &mtime, 0, sizeof(struct timeval) );
   
   if( times == NULL ) {
      
      struct timespec new_time;
      
      int rc = clock_gettime( CLOCK_REALTIME, &new_time );
      if( rc != 0 ) {
         rc = -errno;
         fskit_error("clock_gettime rc = %d\n", rc);
         
         fskit_entry_unlock( fent );
         return rc;
      }
      
      atime.tv_sec = new_time.tv_sec;
      mtime.tv_sec = new_time.tv_sec;
      
      atime.tv_usec = new_time.tv_nsec / 1000;
      mtime.tv_usec = new_time.tv_nsec / 1000;
   }
   else {
      
      atime = times[0];
      mtime = times[1];
   }
   
   fent->atime_sec = atime.tv_sec;
   fent->atime_nsec = atime.tv_usec * 1000;
   
   fent->mtime_sec = mtime.tv_sec;
   fent->mtime_nsec = mtime.tv_usec * 1000;
   
   fskit_entry_unlock( fent );
   return 0;

}