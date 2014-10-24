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

#include "sync.h"
#include "route.h"

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