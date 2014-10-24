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

#include "trunc.h"
#include "route.h"

// run the user-given truncate route callback 
// fent does not have to be locked if it is open, but otherwise it must be at least read-locked
// return 0 on success
// return negative on failure
int fskit_run_user_trunc( struct fskit_core* core, char const* path, struct fskit_entry* fent, off_t new_size, void* handle_data ) {
   
   int rc = 0;
   int cbrc = 0;
   struct fskit_route_dispatch_args dargs;
   
   fskit_route_trunc_args( &dargs, new_size, handle_data );
   
   rc = fskit_route_call_trunc( core, path, fent, &dargs, &cbrc );
   
   if( rc == -EPERM || rc == -ENOSYS ) {
      // can't write, so nothing written
      return 0;
   }
   
   return cbrc;
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
   
   fskit_entry_wlock( fh->fent );
   
   int rc = fskit_do_trunc( core, fh->path, fh->fent, new_size, fh->app_data );
   
   fskit_entry_unlock( fh->fent );
   
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

   rc = fskit_do_trunc( core, path, fent, new_size, NULL );
   
   fskit_entry_unlock( fent );
   
   return rc;
}

// perform a truncate on an inode: run the user truncate route (if given), and update the times and size of the inode.
// return 0 on success 
// return negative on failure 
// fent must be write-locked
int fskit_do_trunc( struct fskit_core* core, char const* path, struct fskit_entry* fent, off_t new_size, void* handle_data ) {
   
   int rc = fskit_run_user_trunc( core, path, fent, new_size, handle_data );
   
   if( rc >= 0 ) {
      
      // update metadata
      fskit_entry_set_mtime( fent, NULL );
      fskit_entry_set_atime( fent, NULL );
      
      fent->size = new_size;
   }
   
   return rc;
}
