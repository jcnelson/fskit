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

#include <fskit/read.h>
#include <fskit/route.h>

#include "fskit_private/private.h"

// run the user-given read route callback
// return the number of bytes read on success
// return negative on failure
ssize_t fskit_run_user_read( struct fskit_core* core, char const* path, struct fskit_entry* fent, char* buf, size_t buflen, off_t offset, void* handle_data ) {

   int rc = 0;
   int cbrc = 0;
   struct fskit_route_dispatch_args dargs;

   fskit_route_io_args( &dargs, buf, buflen, offset, handle_data, NULL );

   rc = fskit_route_call_read( core, path, fent, &dargs, &cbrc );

   if( rc == -EPERM || rc == -ENOSYS ) {
      // no routes installed
      return 0;
   }

   return (ssize_t)cbrc;
}


// read up to buflen bytes into buf, starting at the given offset in the file.
// return the number of bytes read on success.
// return negative on failure.
ssize_t fskit_read( struct fskit_core* core, struct fskit_file_handle* fh, char* buf, size_t buflen, off_t offset ) {

   fskit_file_handle_rlock( fh );

   // sanity check
   if( (fh->flags & O_WRONLY) != 0 ) {

      fskit_file_handle_unlock( fh );
      return -EBADF;
   }

   ssize_t num_read = fskit_run_user_read( core, fh->path, fh->fent, buf, buflen, offset, fh->app_data );

   if( num_read >= 0 ) {

      // update metadata
      fskit_entry_wlock( fh->fent );

      fskit_entry_set_atime( fh->fent, NULL );

      fskit_entry_unlock( fh->fent );
   }

   fskit_file_handle_unlock( fh );

   return num_read;
}
