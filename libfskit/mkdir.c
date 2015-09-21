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

#include <fskit/mkdir.h>
#include <fskit/path.h>
#include <fskit/open.h>
#include <fskit/route.h>
#include <fskit/util.h>

#include "fskit_private/private.h"

// get the user-supplied inode data for creating a directory
int fskit_run_user_mkdir( struct fskit_core* core, char const* path, struct fskit_entry* parent, struct fskit_entry* fent, mode_t mode, void** inode_data ) {

   int rc = 0;
   int cbrc = 0;
   struct fskit_route_dispatch_args dargs;
   char name[FSKIT_FILESYSTEM_NAMEMAX+1];
   
   memset( name, 0, FSKIT_FILESYSTEM_NAMEMAX+1 );
   fskit_basename( path, name );
   
   fskit_route_mkdir_args( &dargs, parent, name, mode );

   rc = fskit_route_call_mkdir( core, path, fent, &dargs, &cbrc );

   if( rc == -EPERM || rc == -ENOSYS ) {
      // no routes
      *inode_data = NULL;
      return 0;
   }

   else if( cbrc != 0 ) {
      // callback failed
      return cbrc;
   }

   else {
      // callback succeded
      *inode_data = dargs.inode_data;
      return 0;
   }
}


// low-level mkdir: create a child and insert it into the parent.
// parent must be a directory
// parent must be write-locked
// return -EEXIST if an entry with the given name (path_basename) already exists in parent.
// return -EIO if we couldn't allocate an inode
// return -ENOMEM if we couldn't allocate memory
static int fskit_mkdir_lowlevel( struct fskit_core* core, char const* path, struct fskit_entry* parent, char const* path_basename, mode_t mode, uint64_t user, uint64_t group ) {

   // resolve the child within the parent
   struct fskit_entry* child = fskit_entry_set_find_name( parent->children, path_basename );
   int err = 0;
   void* app_dir_data = NULL;

   if( child != NULL ) {

      fskit_entry_wlock( child );

      err = fskit_entry_try_garbage_collect( core, path, parent, child );

      if( err >= 0 ) {

         if( err == 0 ) {
            
            // detached but not destroyed
            fskit_entry_unlock( child );
         }

         // name is free
         child = NULL;
      }
      else {
         
         fskit_entry_unlock( child );
         if( err == -EEXIST ) {

            // still exists; can't be garbage-collected
            return -EEXIST;
         }
         else {

            // shouldn't happen
            fskit_error("BUG: fskit_entry_try_garbage_collect(%s) rc = %d\n", path, err );
            return -EIO;
         }
      }
   }

   if( child == NULL ) {

      // create an fskit_entry and attach it
      child = CALLOC_LIST( struct fskit_entry, 1 );
      if( child == NULL ) {
         return -ENOMEM;
      }

      // get an inode for this child
      uint64_t child_inode = fskit_core_inode_alloc( core, parent, child );
      if( child_inode == 0 ) {

         // error in allocation
         fskit_error("fskit_core_inode_alloc(%s) failed\n", path );

         fskit_safe_free( child );

         return -EIO;
      }
      
      // set up the directory
      err = fskit_entry_init_dir( child, parent, child_inode, user, group, mode );
      if( err != 0 ) {
         fskit_error("fskit_entry_init_dir(%s) rc = %d\n", path, err );

         fskit_safe_free( child );
         return err;
      }

      // reference this directory, so it won't disappear during the user's route
      child->open_count++;
      
      // almost done.  run the route callback for this path if needed
      err = fskit_run_user_mkdir( core, path, parent, child, mode, &app_dir_data );
      
      child->open_count--;
      
      if( err != 0 ) {

         // user route failed
         fskit_error("fskit_run_user_mkdir(%s) rc = %d\n", path, err );

         fskit_entry_destroy( core, child, false );
         fskit_safe_free( child );
      }
      else {

         fskit_entry_set_user_data( child, app_dir_data );

         // attach to parent
         fskit_entry_attach_lowlevel( parent, child, path_basename );
      }
   }

   else {

      // already exists
      err = -EEXIST;
   }

   return err;
}


// create a directory
// return -ENOTDIR if one of the elements on the path isn't a directory
// return -EACCES if one of the directories is not searchable
int fskit_mkdir( struct fskit_core* core, char const* path, mode_t mode, uint64_t user, uint64_t group ) {

   int err = 0;

   size_t basename_len = fskit_basename_len( path );
   if( basename_len > FSKIT_FILESYSTEM_NAMEMAX ) {

      return -ENAMETOOLONG;
   }

   // resolve the parent of this child (and write-lock it)
   char* fpath = strdup( path );
   fskit_sanitize_path( fpath );

   char* path_dirname = fskit_dirname( fpath, NULL );
   char* path_basename = fskit_basename( fpath, NULL );

   fskit_sanitize_path( path_dirname );

   fskit_safe_free( fpath );

   struct fskit_entry* parent = fskit_entry_resolve_path( core, path_dirname, user, group, true, &err );

   if( parent == NULL || err ) {

      // parent not found
      fskit_safe_free( path_basename );
      fskit_safe_free( path_dirname );

      // err is set appropriately
      return err;
   }

   if( parent->type != FSKIT_ENTRY_TYPE_DIR ) {

      // parent is not a directory
      fskit_entry_unlock( parent );
      fskit_safe_free( path_basename );
      fskit_safe_free( path_dirname );

      return -ENOTDIR;
   }

   if( !FSKIT_ENTRY_IS_WRITEABLE(parent->mode, parent->owner, parent->group, user, group) ) {

      // parent is not writeable
      fskit_error( "%s is not writable by %" PRIu64 " (%o, %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ")\n", path_dirname, user, parent->mode, parent->owner, parent->group, user, group );

      fskit_entry_unlock( parent );
      fskit_safe_free( path_basename );
      fskit_safe_free( path_dirname );

      return -EACCES;
   }

   err = fskit_mkdir_lowlevel( core, path, parent, path_basename, mode, user, group );

   if( err != 0 ) {
      fskit_error( "fskit_entry_mkdir_lowlevel(%s) rc = %d\n", path, err );
   }

   // clean up
   fskit_entry_unlock( parent );
   fskit_safe_free( path_basename );
   fskit_safe_free( path_dirname );

   return err;
}
