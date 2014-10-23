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

#include "fskit.h"
#include "create.h"
#include "open.h"
#include "route.h"
#include "util.h"

// get the user-supplied inode and handle data for creating a file 
int fskit_run_user_create( struct fskit_core* core, char const* path, struct fskit_entry* fent, int flags, mode_t mode, void** inode_data, void** handle_data ) {
   
   int rc = 0;
   int cbrc = 0;
   struct fskit_route_dispatch_args dargs;
   
   fskit_route_create_args( &dargs, flags, mode );
   
   rc = fskit_route_call_create( core, path, fent, &dargs, &cbrc );
   
   if( rc == -EPERM || rc == -ENOSYS ) {
      // no routes 
      *inode_data = NULL;
      *handle_data = NULL;
      return 0;
   }
   
   else if( cbrc != 0 ) {
      // callback failed 
      return cbrc;
   }
   
   else {
      // callback succeded
      *inode_data = dargs.inode_data;
      *handle_data = dargs.handle_data;
      
      return 0;
   }
}




// do a file create.
// parent must be write-locked
// on success, fill in *ret_child with a newly-created child (which will NOT be locked), and *handle_data with the file handle's app-specific data (generated from the user route)
// also, the child will have been inserted into the parent's children list 
// NOTE: the child will have an open count of 1
int fskit_do_create( struct fskit_core* core, struct fskit_entry* parent, char const* path, int flags, mode_t mode, uint64_t user, uint64_t group, struct fskit_entry** ret_child, void** handle_data ) {

   char path_basename[FSKIT_FILESYSTEM_NAMEMAX + 1];
   void* inode_data = NULL;
   int rc = 0;
   
   memset( path_basename, 0, FSKIT_FILESYSTEM_NAMEMAX + 1 );
   
   fskit_basename( path, path_basename );
   
   // can create--initialize the child
   struct fskit_entry* child = CALLOC_LIST( struct fskit_entry, 1 );
   
   if( child == NULL ) {
      return -ENOMEM;
   }
   
   rc = fskit_entry_init_file( child, 0, path_basename, user, group, mode );

   if( rc != 0 ) {
      fskit_error("fskit_entry_init_file(%s) rc = %d\n", path, rc );
      
      fskit_entry_destroy( core, child, false );
      safe_free( child );
      
      return rc;
   }
   else {
      
      // get an inode for this file 
      uint64_t child_inode = fskit_core_inode_alloc( core, parent, child );
      if( child_inode == 0 ) {
         // error in allocating 
         fskit_error("fskit_core_inode_alloc(%s) failed\n", path );
         
         fskit_entry_destroy( core, child, false );
         safe_free( child );
         
         return -EIO;
      }
      
      // Generate any app data we need to 
      rc = fskit_run_user_create( core, path, child, flags, mode, &inode_data, handle_data );
      if( rc != 0 ) {
         
         // callback error 
         fskit_error("fskit_run_user_create(%s) rc = %d\n", path, rc );
         
         fskit_entry_destroy( core, child, false );
         safe_free( child );
         
         return rc;
      }
      
      // insert app data 
      fskit_entry_set_user_data( child, inode_data );
      
      // insert it into the filesystem
      fskit_entry_wlock( child );
      
      child->file_id = child_inode;
      
      // open it
      child->open_count++;
      
      fskit_entry_attach_lowlevel( parent, child );
      
      fskit_entry_unlock( child );
      
      *ret_child = child;
      
      // update the number of files 
      fskit_file_count_update( core, 1 );
      
      return 0;
   }
}


// create an entry (equivalent to open with O_CREAT|O_WRONLY|O_TRUNC)
struct fskit_file_handle* fskit_create( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, mode_t mode, int* err ) {
   return fskit_open( core, path, user, group, O_CREAT|O_WRONLY|O_TRUNC, mode, err );
}
