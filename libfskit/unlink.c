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
// return the usual path resolution errors
// return 1 if the file inode data was completely destroyed
static int fskit_unlink_ex( struct fskit_core* core, char const* path, uint64_t owner, uint64_t group, bool writelock_child ) {

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

   free( path_basename );

   if( fent == NULL ) {

      fskit_entry_unlock( parent );

      return -ENOENT;
   }
   
   if( writelock_child ) {
      fskit_entry_wlock( fent );
   }

   // detatch fent from parent (don't worry if someone raced ahead of us)
   rc = fskit_entry_detach_lowlevel( parent, fent );
   if( rc != 0 && rc != -ENOENT ) {

      fskit_error("fskit_entry_detach_lowlevel(%p) rc = %d\n", fent, rc );

      if( writelock_child ) {
         fskit_entry_unlock( fent );
      }
      
      fskit_entry_unlock( parent );
      return rc;
   }
   
   // try to destroy fent
   // note that this unlocks fent and re-locks it if it is fully unref'ed (i.e. no path resolves to it)
   rc = fskit_entry_try_destroy( core, path, fent );
   if( rc > 0 ) {

      // destroyed
      fskit_safe_free( fent );
      
      // update number of files
      fskit_file_count_update( core, -1 );
   }
   else if( rc < 0 ) {

      // some error occurred
      fskit_error("fskit_entry_try_destroy(%p) rc = %d\n", fent, rc );

      if( writelock_child ) {
         fskit_entry_unlock( fent );
      }
      
      fskit_entry_unlock( parent );
      return rc;
   }
   else if( writelock_child ) {

      // done with this entry
      fskit_entry_unlock( fent );
   }

   fskit_entry_unlock( parent );

   return rc;
}

// unlink a file from the filesystem 
// return path resolution errors, and -ENOENT if unlinking fails for some reason 
int fskit_unlink( struct fskit_core* core, char const* path, uint64_t owner, uint64_t group ) {

   int rc = fskit_unlink_ex( core, path, owner, group, true );
   if( rc == 1 ) {
      rc = 0;
   }
   
   return rc;
}

// unlink a file from the filessytem, but don't lock the child to unlink 
// return path resolution errors, and -ENOENT if unlinking fails for some reason 
// return 1 if th file inode data was completely destroyed
int fskit_unlink_nolock( struct fskit_core* core, char const* path, uint64_t owner, uint64_t group ) {
   
   return fskit_unlink_ex( core, path, owner, group, false );
}
   
