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

#include <fskit/closedir.h>
#include <fskit/close.h>
#include <fskit/util.h>

// free a directory handle
static void fskit_dir_handle_destroy( struct fskit_dir_handle* dirh ) {

   dirh->dent = NULL;

   if( dirh->path != NULL ) {
      fskit_safe_free( dirh->path );
      dirh->path = NULL;
   }

   pthread_rwlock_destroy( &dirh->lock );

   memset( dirh, 0, sizeof(struct fskit_dir_handle) );

   fskit_safe_free( dirh );
}


// close a directory handle, freeing it.
// a directory may be unlinked on close, if this was the last handle to it, and its link count was zero.
// if this happens, then app_dir_data will contain the directory's app data, and the directory will be freed.
// return 0 on success
// return the following errors:
// * EBADF if the directory handle is invalid
// * EDEADLK if there is a bug in the lock handling
int fskit_closedir( struct fskit_core* core, struct fskit_dir_handle* dirh ) {

   int rc = 0;

   rc = fskit_dir_handle_wlock( dirh );
   if( rc != 0 ) {
      // indicates deadlock; shouldn't happen
      fskit_error("BUG: fskit_dir_handle_wlock(%p) rc = %d\n", dirh, rc );
      return rc;
   }

   if( dirh->dent == NULL ) {

      fskit_dir_handle_unlock( dirh );
      return -EBADF;
   }

   rc = fskit_entry_wlock( dirh->dent );
   if( rc != 0 ) {

      // shouldn't happen; indicates deadlock
      fskit_error("BUG: fskit_entry_wlock(%p) rc = %d\n", dirh->dent, rc );

      fskit_dir_handle_unlock( dirh );
      return rc;
   }

   // run user-given close route
   rc = fskit_run_user_close( core, dirh->path, dirh->dent, dirh->app_data );
   if( rc != 0 ) {

      fskit_error("fskit_run_user_close(%s) rc = %d\n", dirh->path, rc );

      fskit_entry_unlock( dirh->dent );
      fskit_dir_handle_unlock( dirh );
      return rc;
   }

   // no longer open
   dirh->dent->open_count --;

   // see if we can destroy this....
   rc = fskit_entry_try_destroy( core, dirh->path, dirh->dent );
   if( rc > 0 ) {

      // dent was unlocked and destroyed
      fskit_safe_free( dirh->dent );
      rc = 0;
   }
   else if( rc < 0 ) {

      // some error occurred
      fskit_error("fskit_entry_try_destroy(%p) rc = %d\n", dirh->dent, rc );
      fskit_entry_unlock( dirh->dent );

      return rc;
   }
   else {
      // done with this directory
      fskit_entry_unlock( dirh->dent );
   }

   // get rid of this handle
   fskit_dir_handle_unlock( dirh );
   fskit_dir_handle_destroy( dirh );

   return 0;
}
