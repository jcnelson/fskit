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

#include "mknod.h"
#include "path.h"
#include "open.h"
#include "route.h"

// get the user-supplied inode data for creating a node 
int fskit_run_user_mknod( struct fskit_core* core, char const* path, struct fskit_entry* fent, mode_t mode, dev_t dev, void** inode_data ) {
   
   int rc = 0;
   int cbrc = 0;
   struct fskit_route_dispatch_args dargs;
   
   fskit_route_mknod_args( &dargs, mode, dev );
   
   rc = fskit_route_call_mknod( core, path, fent, &dargs, &cbrc );
   
   if( rc == -EPERM ) {
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


// make a node
int fskit_mknod( struct fskit_core* core, char const* path, mode_t mode, dev_t dev, uint64_t user, uint64_t group ) {
   
   int err = 0;
   int rc = 0;
   void* inode_data = NULL;

   // get the parent directory and lock it
   char* path_dirname = fskit_dirname( path, NULL );
   struct fskit_entry* parent = fskit_entry_resolve_path( core, path_dirname, user, group, true, &err );
   
   safe_free( path_dirname );
   
   if( err != 0 || parent == NULL ) {
      return err;
   }

   if( !FSKIT_ENTRY_IS_DIR_READABLE( parent->mode, parent->owner, parent->group, user, group ) ) {
      // not searchable
      fskit_entry_unlock( parent );
      return -EACCES;
   }

   if( !FSKIT_ENTRY_IS_WRITEABLE( parent->mode, parent->owner, parent->group, user, group ) ) {
      // not writeable
      fskit_entry_unlock( parent );
      return -EACCES;
   }

   char* path_basename = fskit_basename( path, NULL );

   // make sure it doesn't exist already (or isn't in the process of being deleted, since we might have to re-create it if deleting it fails)
   if( fskit_entry_set_find_name( parent->children, path_basename ) != NULL ) {
      fskit_entry_unlock( parent );
      safe_free( path_basename );
      return -EEXIST;
   }

   struct fskit_entry* child = CALLOC_LIST( struct fskit_entry, 1 );
   
   mode_t mmode = 0;
   char const* method_name = NULL;
   
   if( S_ISREG(mode) ) {
      
      // regular file 
      mmode = (mode & 0777) | S_IFREG;
      method_name = "fskit_entry_init_file";
      
      err = fskit_entry_init_file( child, 0, path_basename, user, group, mmode );
   }
   else if( S_ISFIFO(mode) ) {
      
      // fifo 
      mmode = (mode & 0777) | S_IFIFO;
      method_name = "fskit_entry_init_fifo";
      
      err = fskit_entry_init_fifo( child, 0, path_basename, user, group, mmode );
   }
   else if( S_ISSOCK(mode) ) {
      
      // socket 
      mmode = (mode & 0777) | S_IFSOCK;
      method_name = "fskit_entry_init_sock";
      
      err = fskit_entry_init_sock( child, 0, path_basename, user, group, mmode );
   }
   else if( S_ISCHR(mode) ) {
      
      // character device 
      mmode = (mode & 0777) | S_IFCHR;
      method_name = "fskit_entry_init_chr";
      
      err = fskit_entry_init_chr( child, 0, path_basename, user, group, mmode, dev );
   }
   else if( S_ISBLK(mode) ) {
      
      // block device 
      mmode = (mode & 0777) | S_IFBLK;
      method_name = "fskit_entry_init_blk";
      
      err = fskit_entry_init_blk( child, 0, path_basename, user, group, mmode, dev );
   }
   else {
      
      errorf("Invalid/unsupported mode %o\n", mode );
      
      fskit_entry_unlock( parent );
      safe_free( path_basename );
      fskit_entry_destroy( core, child, false );
      safe_free( child );
      
      return -EINVAL;
   }
   
   if( err == 0 ) {
      
      // success! get the inode number 
      uint64_t file_id = fskit_core_inode_alloc( core, parent, child );
      if( file_id == 0 ) {
         
         errorf("fskit_core_inode_alloc(%s) failed\n", path );
         
         fskit_entry_unlock( parent );
         safe_free( path_basename );
         fskit_entry_destroy( core, child, false );
         safe_free( child );
         
         return -EIO;
      }
      
      fskit_entry_wlock( child );
      
      child->file_id = file_id;
      
      // perform any user-defined creations 
      rc = fskit_run_user_mknod( core, path, child, mode, dev, &inode_data );
      if( rc != 0 ) {
         
         // failed. abort creation
         errorf("fskit_run_user_mknod(%s) rc = %d\n", path, rc );
         
         fskit_entry_unlock( parent );
         safe_free( path_basename );
         fskit_entry_destroy( core, child, true );
         safe_free( child );
         
         return rc;
      }
      
      // attach the file
      fskit_entry_attach_lowlevel( parent, child );
      
      fskit_entry_unlock( child );
   }
   else {
      errorf("%s(%s) rc = %d\n", method_name, path, err );
      fskit_entry_destroy( core, child, false );
      safe_free( child );
   }
   
   fskit_entry_unlock( parent );

   safe_free( path_basename );

   return err;
}
