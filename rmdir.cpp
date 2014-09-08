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

#include "rmdir.h"
#include "path.h"

// remove a directory, if it is empty.  On detach, if the child is no longer open, then also put its app-defined data into *app_dir_data for the caller to deal with.
// return 0 on success
// on error, return one of the following:
// * -ENAMETOOLONG if the path is too long
// * -EACCES if one of the directories along the path can't be searched
// * -ENOTDIR if one of the directories along the path isn't a directory
// * -ENOENT if one of the directories along the path doesn't exist
// * -ENOTEMPTY if the directory isn't empty
int fskit_entry_rmdir( struct fskit_core* core, char const* _path, uint64_t user, uint64_t group, void** app_dir_data ) {

   // get some info about this directory first
   int rc = 0;
   
   if( strlen(_path) >= PATH_MAX ) {
      return -ENAMETOOLONG;
   }
   
   // ensure path ends in /
   char path[PATH_MAX];
   memset( path, 0, PATH_MAX );
   strncpy( path, _path, PATH_MAX - 1 );

   fskit_sanitize_path( path );
   
   // write-lock the directory
   int err = 0;
   struct fskit_entry* dent = fskit_entry_resolve_path( core, path, user, group, true, &err );
   if( !dent || err ) {
      return err;
   }
   
   // is this a directory?
   if( dent->type != FSKIT_ENTRY_TYPE_DIR ) {
      fskit_entry_unlock( dent );
      return -ENOTDIR;
   }

   char* path_dirname = fskit_dirname( path, NULL );

   // write-lock the parent
   struct fskit_entry* parent = fskit_entry_resolve_path( core, path_dirname, user, group, true, &err );

   free( path_dirname );

   // parent not found?
   if( !parent || err ) {
      fskit_entry_unlock( dent );

      return err;
   }

   // IS THE PARENT EMPTY?
   if( fskit_entry_set_count( dent->children ) > 2 ) {
      // nope
      fskit_entry_unlock( dent );
      fskit_entry_unlock( parent );

      return -ENOTEMPTY;
   }

   // empty. Detach from the filesystem 
   rc = fskit_entry_detach_lowlevel( parent, dent );
   if( rc != 0 ) {
      errorf("fskit_entry_detach_lowlevel(%p) rc = %d\n", dent, rc );
      
      fskit_entry_unlock( dent );
      fskit_entry_unlock( parent );
      return rc;
   }
   
   // try to destroy?
   rc = fskit_entry_try_destroy( dent, app_dir_data );
   if( rc > 0 ) {
      
      // destroyed 
      free( dent );
      rc = 0;
   }
   else if( rc < 0 ) {
      
      errorf("fskit_try_destroy(%p) rc = %d\n", dent, rc );
      fskit_entry_unlock( dent );
   }
   else {
      
      // done with this entry 
      fskit_entry_unlock( dent );
   }
   
   fskit_entry_unlock( parent );
   
   return rc;
}

