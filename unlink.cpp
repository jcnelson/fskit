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

#include "unlink.h"
#include "path.h"

// unlink a file from the filesystem
// return the usual path resolution errors 
int fskit_unlink( struct fskit_core* core, char const* path, uint64_t owner, uint64_t group ) {
   
   // get some info about this file first
   int rc = 0;
   int err = 0;
   
   // get the fent, and write-lock it
   struct fskit_entry* fent = fskit_entry_resolve_path( core, path, owner, group, true, &err );
   if( !fent || err ) {
      return err;
   }

   // look up the parent and write-lock it
   char* path_dirname = fskit_dirname( path, NULL );
   
   struct fskit_entry* parent = fskit_entry_resolve_path( core, path_dirname, owner, group, true, &err );

   free( path_dirname );

   
   if( !parent || err ) {
      fskit_entry_unlock( fent );

      return err;
   }
   
   // mark the file as deleted, so it won't show up again in any listing 
   fent->deletion_in_progress = true;
   
   // detatch fent from parent
   rc = fskit_entry_detach_lowlevel( parent, fent );
   if( rc != 0 ) {
      
      errorf("fskit_entry_detach_lowlevel(%p) rc = %d\n", fent, rc );
      
      fskit_entry_unlock( fent );
      fskit_entry_unlock( parent );
      return rc;
   }
   
   // try to destroy fent 
   rc = fskit_entry_try_destroy( core, path, fent );
   if( rc > 0 ) {
      
      // destroyed 
      free( fent );
      rc = 0;
   }
   else if( rc < 0 ) {
   
      // some error occurred 
      errorf("fskit_entry_try_destroy(%p) rc = %d\n", fent, rc );
      
      fskit_entry_unlock( fent );
      fskit_entry_unlock( parent );
      return rc;
   }
   else {
      
      // done with this entry 
      fskit_entry_unlock( fent );
   }
   
   fskit_entry_unlock( parent );
   
   if( rc == 0 ) {
      // update number of files
      fskit_file_count_update( core, -1 );
   }
   
   
   return rc;
}

