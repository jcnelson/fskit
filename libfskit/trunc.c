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

#include <fskit/trunc.h>
#include <fskit/route.h>
#include <fskit/util.h>

#include "fskit_private/private.h"


// i/o continuation, called with the same locks held as the trunc()
static int fskit_trunc_cont( struct fskit_core* core, struct fskit_entry* fent, off_t new_size, ssize_t trunc_rc ) {

   if( trunc_rc == 0 ) {

      // update metadata
      fskit_entry_set_mtime( fent, NULL );
      fskit_entry_set_atime( fent, NULL );

      fent->size = new_size;
   }

   return 0;
}

// run the user-given truncate route callback
// fent should be referenced, but it should NOT be locked in any way
// return 0 on success
// return negative on failure
int fskit_run_user_trunc( struct fskit_core* core, char const* path, struct fskit_entry* fent, off_t new_size, void* handle_data ) {

   int rc = 0;
   int cbrc = 0;
   struct fskit_route_dispatch_args dargs;

   fskit_route_trunc_args( &dargs, new_size, handle_data, fskit_trunc_cont );

   rc = fskit_route_call_trunc( core, path, fent, &dargs, &cbrc );

   if( rc == -EPERM || rc == -ENOSYS ) {
      // no routes installed.
      return 0;
   }

   return cbrc;
}


// directly set the size.
// do not call the user callback.
// always succeeds
// NOTE; fent must be write-locked
int fskit_entry_set_size( struct fskit_entry* fent, off_t size ) {
   
   fent->size = size;
   return 0;
}

// truncate a file to a given size
// return zero on success
// return negative on failure.
int fskit_ftrunc( struct fskit_core* core, struct fskit_file_handle* fh, off_t new_size ) {

   fskit_file_handle_rlock( fh );

   // sanity check
   if( (fh->flags & (O_RDWR | O_WRONLY)) == 0 ) {

      fskit_file_handle_unlock( fh );
      return -EBADF;
   }

   int rc = fskit_run_user_trunc( core, fh->path, fh->fent, new_size, fh->app_data );

   fskit_file_handle_unlock( fh );

   return rc;
}


// truncate a file to a given size
// return 0 on success
// return negative on failure
int fskit_trunc( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, off_t new_size ) {

   int err = 0;
   int rc = 0;

   // get the fent
   struct fskit_entry* fent = fskit_entry_resolve_path( core, path, user, group, true, &err );
   if( fent == NULL || err != 0 ) {
      return err;
   }

   // reference the fent, so it won't go anywhere
   fent->open_count++;

   fskit_entry_unlock( fent );

   rc = fskit_run_user_trunc( core, path, fent, new_size, NULL );

   // unreference
   fskit_entry_wlock( fent );

   fent->open_count--;

   // need to free?  note that this may unlock and re-lock fent, but only if it cannot be resolved by any path
   rc = fskit_entry_try_destroy( core, path, fent );
   if( rc > 0 ) {

      // fent was unlocked and destroyed
      fskit_safe_free( fent );
      rc = 0;

      fskit_file_count_update( core, -1 );
   }
   else if( rc < 0 ) {

      // some error occurred
      fskit_error("fskit_entry_try_destroy(%p) rc = %d\n", fent, rc );
      fskit_entry_unlock( fent );

      return rc;
   }
   else {

      // done with this entry
      fskit_entry_unlock( fent );
   }

   return rc;
}
