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

#include <fskit/close.h>
#include <fskit/route.h>
#include <fskit/util.h>

#include "fskit_private/private.h"

// destroy a file handle
// the file handle must be write-locked
// this calls the route for close as well.
// in all cases, the file handle is destroyed, but this method will return negative if the route callback failed.
static int fskit_file_handle_destroy( struct fskit_file_handle* fh ) {

   fh->fent = NULL;

   if( fh->path ) {
      fskit_safe_free( fh->path );
      fh->path = NULL;
   }

   pthread_rwlock_destroy( &fh->lock );

   memset( fh, 0, sizeof(struct fskit_file_handle) );

   fskit_safe_free( fh );

   return 0;
}


// run the user-installed close handler
// return 0 on success, or if there are no routes
// return negative on callback failure
int fskit_run_user_close( struct fskit_core* core, char const* path, struct fskit_entry* fent, void* handle_data ) {

   // route?
   struct fskit_route_dispatch_args dargs;
   int rc = 0;
   int cbrc = 0;

   fskit_route_close_args( &dargs, handle_data );
   rc = fskit_route_call_close( core, path, fent, &dargs, &cbrc );

   if( rc == -EPERM || rc == -ENOSYS ) {

      // no routes
      return 0;
   }
   else if( cbrc != 0 ) {

      // callback failed
      return cbrc;
   }
   else {

      // success!
      return 0;
   }
}

// close a file descriptor.  This will free it, so don't use it again.  On success, the handle's app-specific data will be placed into *app_handle_data.
// a file may be destroyed by closing it, if it was unlinked.  If so, *app_file_data will contain its app-specific data.
// return 0 on success
// return -EBADF if the handle is invalid
// return -EDEADLK of there is a bug in the lock handling
int fskit_close( struct fskit_core* core, struct fskit_file_handle* fh ) {

   int rc = 0;

   rc = fskit_file_handle_wlock( fh );
   if( rc != 0 ) {
      // shouldn't happen; indicates deadlock
      fskit_error("BUG: fskit_file_handle_wlock(%p) rc = %d\n", fh, rc );
      return rc;
   }

   // validity?
   if( fh->fent == NULL ) {
      fskit_file_handle_unlock( fh );
      return -EBADF;
   }

   // clean up the handle
   rc = fskit_run_user_close( core, fh->path, fh->fent, fh->app_data );
   if( rc != 0 ) {
      // failed to run user close
      fskit_error("fskit_run_user_close(%s) rc = %d\n", fh->path, rc );

      fskit_entry_unlock( fh->fent );
      fskit_file_handle_unlock( fh );
      return rc;
   }
   
   rc = fskit_entry_wlock( fh->fent );
   if( rc != 0 ) {
      // shouldn't happen: indicates deadlock
      fskit_error("BUG: fskit_entry_wlock(%p) rc = %d\n", fh->fent, rc );
      fskit_file_handle_unlock( fh );
      return rc;
   }

   // no longer open by this handle
   fh->fent->open_count--;

   // maybe this entry has been fully unref'ed?
   // this may unlock fh->fent and re-lock it, but only if fent is already fully unlinked
   rc = fskit_entry_try_destroy_and_free( core, fh->path, fh->fent );

   if( rc < 0 ) {

      // some error occurred
      fskit_error("fskit_entry_try_destroy(%p) rc = %d\n", fh->fent, rc );
      fskit_entry_unlock( fh->fent );

      return rc;
   }
   else if( rc == 0 ) {

      // done with this entry--it's still exists
      fskit_entry_unlock( fh->fent );
   }
   else {

      // destroyed and freed
      fh->fent = NULL;
      rc = 0;
   }

   // get rid of this handle
   fskit_file_handle_unlock( fh );
   fskit_file_handle_destroy( fh );

   return rc;
}
