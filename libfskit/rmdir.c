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

#include <fskit/rmdir.h>
#include <fskit/path.h>
#include <fskit/util.h>

#include "fskit_private/private.h"


// remove a directory, if it is empty.
// return 0 on success
// on error, return one of the following:
// * -ENAMETOOLONG if the path is too long
// * -EACCES if one of the directories along the path can't be searched
// * -ENOTDIR if one of the directories along the path isn't a directory
// * -ENOENT if one of the directories along the path doesn't exist
// * -ENOTEMPTY if the directory isn't empty
int fskit_rmdir( struct fskit_core* core, char const* _path, uint64_t user, uint64_t group ) {

   // get some info about this directory first
   int rc = 0;

   if( strlen(_path) >= PATH_MAX ) {
      return -ENAMETOOLONG;
   }

   if( fskit_basename_len(_path) > FSKIT_FILESYSTEM_NAMEMAX ) {
      return -ENAMETOOLONG;
   }

   // ensure path ends in /
   char path[PATH_MAX];
   memset( path, 0, PATH_MAX );
   strncpy( path, _path, PATH_MAX - 1 );

   fskit_sanitize_path( path );

   // look up the parent and write-lock it
   char* path_dirname = fskit_dirname( path, NULL );
   char* path_basename = fskit_basename( path, NULL );

   struct fskit_entry* parent = fskit_entry_resolve_path( core, path_dirname, user, group, true, &rc );

   free( path_dirname );

   if( !parent || rc ) {

      fskit_entry_unlock( parent );

      free( path_basename );

      return rc;
   }

   // is the parent a directory?
   if( parent->type != FSKIT_ENTRY_TYPE_DIR ) {
      // nope
      fskit_entry_unlock( parent );

      free( path_basename );

      return -ENOTDIR;
   }

   // find the directory, and write-lock it
   struct fskit_entry* dent = fskit_entry_set_find_name( parent->children, path_basename );

   if( dent == NULL ) {

      fskit_entry_unlock( parent );
      fskit_safe_free( path_basename );

      return -ENOENT;
   }

   fskit_entry_wlock( dent );

   // is this a directory?
   if( dent->type != FSKIT_ENTRY_TYPE_DIR ) {
      // nope
      fskit_entry_unlock( dent );
      fskit_entry_unlock( parent );
      
      fskit_safe_free( path_basename );

      return -ENOTDIR;
   }

   // IS THE PARENT EMPTY?
   if( fskit_entry_set_count( dent->children ) > 2 ) {
      // nope
      fskit_entry_unlock( dent );
      fskit_entry_unlock( parent );
      fskit_safe_free( path_basename );
      
      return -ENOTEMPTY;
   }

   // empty. Detach from the filesystem
   rc = fskit_entry_detach_lowlevel( parent, path_basename );
   fskit_safe_free( path_basename );
   
   if( rc != 0 ) {
      fskit_error("fskit_entry_detach_lowlevel(%p) rc = %d\n", dent, rc );

      fskit_entry_unlock( dent );
      fskit_entry_unlock( parent );
      
      return rc;
   }

   // try to destroy?
   // NOTE: this will unlock and free dent if it succeeds
   rc = fskit_entry_try_destroy_and_free( core, path, parent, dent );
   if( rc > 0 ) {

      // destroyed
      dent = NULL;
      rc = 0;
   }
   else if( rc < 0 ) {

      fskit_error("fskit_try_destroy(%p) rc = %d\n", dent, rc );
      fskit_entry_unlock( dent );
   }
   else {

      // not destroyed
      // done with this entry
      fskit_entry_unlock( dent );
   }

   fskit_entry_unlock( parent );

   return rc;
}
