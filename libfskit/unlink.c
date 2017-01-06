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

#include <fskit/unlink.h>
#include <fskit/path.h>
#include <fskit/util.h>

#include "fskit_private/private.h"


// unlink a file from the filesystem
// return 0 on success
// return the usual path resolution errors
int fskit_unlink( struct fskit_core* core, char const* path, uint64_t owner, uint64_t group ) {

   // get some info about this file first
   int rc = 0;
   int err = 0;

   // look up the parent and write-lock it
   char* path_dirname = fskit_dirname( path, NULL );
   char* path_basename = fskit_basename( path, NULL );
   
   if( path_basename == NULL || path_dirname == NULL ) {
      
      fskit_safe_free( path_basename );
      fskit_safe_free( path_dirname );
      
      return -ENOMEM;
   }

   if( strlen(path_basename) > FSKIT_FILESYSTEM_NAMEMAX ) {

      fskit_safe_free( path_basename );
      fskit_safe_free( path_dirname );
      return -ENAMETOOLONG;
   }

   struct fskit_entry* parent = fskit_entry_resolve_path( core, path_dirname, owner, group, true, &err );

   free( path_dirname );

   if( !parent || err ) {

      fskit_entry_unlock( parent );

      free( path_basename );

      return err;
   }

   // is the parent a directory?
   if( parent->type != FSKIT_ENTRY_TYPE_DIR ) {
      // nope
      fskit_entry_unlock( parent );

      free( path_basename );

      return -ENOTDIR;
   }

   // find the fent, and write-lock it
   struct fskit_entry* fent = fskit_entry_set_find_name( parent->children, path_basename );

   if( fent == NULL ) {

      fskit_entry_unlock( parent );
      free( path_basename );
      return -ENOENT;
   }
   
   // detach fent from parent
   rc = fskit_entry_detach_lowlevel( parent, path_basename );
   free( path_basename );
   
   if( rc != 0 && rc != -ENOENT ) {

      fskit_error("fskit_entry_detach_lowlevel(%p) rc = %d\n", fent, rc );

      fskit_entry_unlock( parent );
      return rc;
   }

   // user detach handler
   rc = fskit_run_user_detach( core, path, parent, fent );
   if( rc < 0 ) {
        
       fskit_error("fskit_run_user_detach('%s') rc = %d\n", path, rc );
       rc = 0;
   }
   
   fskit_entry_wlock( fent );
   
   // try to destroy fent
   // note that this unlocks fent and destroys it if it is fully unref'ed
   rc = fskit_entry_try_destroy_and_free( core, path, parent, fent );
   if( rc > 0 ) {

      // destroyed
      fent = NULL;
      rc = 0;
   }
   else if( rc < 0 ) {

      // some error occurred
      fskit_error("fskit_entry_try_destroy_and_free(%p) rc = %d\n", fent, rc );
      fskit_entry_unlock( fent );
   }
   else {

      // done with this entry
      fskit_entry_unlock( fent );
   }

   fskit_entry_unlock( parent );

   return rc;
}
