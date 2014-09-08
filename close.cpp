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

#include "close.h"

// destroy a file handle
// the file handle must be write-locked
static int fskit_file_handle_destroy( struct fskit_file_handle* fh ) {
   
   fh->fent = NULL;
   
   if( fh->path ) {
      free( fh->path );
      fh->path = NULL;
   }
   
   pthread_rwlock_destroy( &fh->lock );
   
   memset( fh, 0, sizeof(struct fskit_file_handle) );
   
   free( fh );
   
   return 0;
}


// close a file descriptor.  This will free it, so don't use it again.  On success, the handle's app-specific data will be placed into *app_handle_data.
// a file may be destroyed by closing it, if it was unlinked.  If so, *app_file_data will contain its app-specific data.
// return 0 on success
// return -EBADF if the handle is invalid
// return -EDEADLK of there is a bug in the lock handling
int fskit_close( struct fskit_file_handle* fh, void** app_handle_data, void** app_file_data ) {
   
   int rc = 0;
   
   rc = fskit_file_handle_wlock( fh );
   if( rc != 0 ) {
      // shouldn't happen; indicates deadlock 
      errorf("BUG: fskit_file_handle_wlock(%p) rc = %d\n", fh, rc );
      return rc;
   }
   
   // validity?
   if( fh->fent == NULL ) {
      fskit_file_handle_unlock( fh );
      return -EBADF;
   }

   rc = fskit_entry_wlock( fh->fent );
   if( rc != 0 ) {
      // shouldn't happen: indicates deadlock 
      errorf("BUG: fskit_entry_wlock(%p) rc = %d\n", fh->fent, rc );
      fskit_file_handle_unlock( fh );
      return rc;
   }

   // no longer open by this handle
   fh->fent->open_count--;

   // see if we can destroy this....
   rc = fskit_entry_try_destroy( fh->fent, app_file_data );
   if( rc > 0 ) {
      
      // fent was unlocked and destroyed
      free( fh->fent );
      rc = 0;
   }
   else if( rc < 0 ) {
      
      // some error occurred 
      errorf("fskit_entry_try_destroy(%p) rc = %d\n", fh->fent, rc );
      fskit_entry_unlock( fh->fent );
      
      return rc;
   }
   else {
      
      // done with this entry
      fskit_entry_unlock( fh->fent );
   }
   
   // give back the app data 
   if( app_handle_data != NULL ) {
      *app_handle_data = fh->app_data;
   }

   // get rid of this handle
   fskit_file_handle_unlock( fh );
   fskit_file_handle_destroy( fh );
   
   return rc;

}