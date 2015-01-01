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

#include <fskit/sync.h>
#include <fskit/route.h>

static int fskit_do_user_sync( struct fskit_core* core, char const* path, struct fskit_entry* fent ) {

   int rc = 0;
   int cbrc = 0;

   struct fskit_route_dispatch_args dargs;

   fskit_route_sync_args( &dargs );

   rc = fskit_route_call_sync( core, path, fent, &dargs, &cbrc );

   if( rc == -EPERM || rc == -ENOSYS ) {
      // no stat defined
      return 0;
   }

   return cbrc;
}


// sync a file handle
// basically, just call the user route, but write-lock the entry
int fskit_fsync( struct fskit_core* core, struct fskit_file_handle* fh ) {

   fskit_file_handle_rlock( fh );
   fskit_entry_wlock( fh->fent );

   int rc = fskit_do_user_sync( core, fh->path, fh->fent );

   fskit_entry_unlock( fh->fent );
   fskit_file_handle_unlock( fh );

   return rc;
}
