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

#include <fskit/mknod.h>
#include <fskit/path.h>
#include <fskit/open.h>
#include <fskit/route.h>
#include <fskit/util.h>

#include "fskit_private/private.h"

// get the user-supplied inode data for creating a node
int fskit_run_user_mknod( struct fskit_core* core, char const* path, struct fskit_entry* parent, struct fskit_entry* fent, mode_t mode, dev_t dev, void* cls, void** inode_data ) {

   int rc = 0;
   int cbrc = 0;
   char name[FSKIT_FILESYSTEM_NAMEMAX+1];
   struct fskit_route_dispatch_args dargs;
   
   memset( name, 0, FSKIT_FILESYSTEM_NAMEMAX+1 );
   fskit_basename( path, name );

   fskit_route_mknod_args( &dargs, parent, name, mode, dev, cls );

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
int fskit_mknod_ex( struct fskit_core* core, char const* fs_path, mode_t mode, dev_t dev, uint64_t user, uint64_t group, void* cls ) {

   int err = 0;
   void* inode_data = NULL;
   struct fskit_entry* child = NULL;

   // sanity check
   size_t basename_len = fskit_basename_len( fs_path );
   if( basename_len > FSKIT_FILESYSTEM_NAMEMAX ) {

      return -ENAMETOOLONG;
   }

   char* path = strdup( fs_path );
   fskit_sanitize_path( path );

   // get the parent directory and lock it
   char* path_dirname = fskit_dirname( path, NULL );
   struct fskit_entry* parent = fskit_entry_resolve_path( core, path_dirname, user, group, true, &err );

   fskit_safe_free( path_dirname );

   if( err != 0 || parent == NULL ) {

      fskit_safe_free( path );
      return err;
   }

   if( !FSKIT_ENTRY_IS_DIR_SEARCHABLE( parent->mode, parent->owner, parent->group, user, group ) ) {
      // not searchable
      fskit_entry_unlock( parent );
      fskit_safe_free( path );
      return -EACCES;
   }

   if( !FSKIT_ENTRY_IS_WRITEABLE( parent->mode, parent->owner, parent->group, user, group ) ) {
      // not writeable
      fskit_entry_unlock( parent );
      fskit_safe_free( path );
      return -EACCES;
   }

   char* path_basename = fskit_basename( path, NULL );

   child = fskit_entry_set_find_name( parent->children, path_basename );

   if( child != NULL ) {

      fskit_entry_wlock( child );

      // it might have been marked for garbage-collection
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

         // can't garbage-collect
         fskit_entry_unlock( parent );
         fskit_entry_unlock( child );
         fskit_safe_free( path );

         if( err == -EEXIST ) {
            return -EEXIST;
         }
         else {

            // shouldn't happen
            fskit_error("BUG: fskit_entry_try_garbage_collect(%s) rc = %d\n", path, err );
            return -EIO;
         }
      }
   }

   child = CALLOC_LIST( struct fskit_entry, 1 );

   mode_t mmode = 0;
   char const* method_name = NULL;

   if( S_ISREG(mode) ) {

      // regular file
      mmode = (mode & 0777) | S_IFREG;
      method_name = "fskit_entry_init_file";

      err = fskit_entry_init_file( child, 0, user, group, mmode );
   }
   else if( S_ISFIFO(mode) ) {

      // fifo
      mmode = (mode & 0777) | S_IFIFO;
      method_name = "fskit_entry_init_fifo";

      err = fskit_entry_init_fifo( child, 0, user, group, mmode );
   }
   else if( S_ISSOCK(mode) ) {

      // socket
      mmode = (mode & 0777) | S_IFSOCK;
      method_name = "fskit_entry_init_sock";

      err = fskit_entry_init_sock( child, 0, user, group, mmode );
   }
   else if( S_ISCHR(mode) ) {

      // character device
      mmode = (mode & 0777) | S_IFCHR;
      method_name = "fskit_entry_init_chr";

      err = fskit_entry_init_chr( child, 0, user, group, mmode, dev );
   }
   else if( S_ISBLK(mode) ) {

      // block device
      mmode = (mode & 0777) | S_IFBLK;
      method_name = "fskit_entry_init_blk";

      err = fskit_entry_init_blk( child, 0, user, group, mmode, dev );
   }
   else {

      fskit_error("Invalid/unsupported mode %o\n", mode );

      fskit_entry_unlock( parent );
      fskit_safe_free( path_basename );
      fskit_entry_destroy( core, child, false );
      fskit_safe_free( child );
      fskit_safe_free( path );

      return -EINVAL;
   }

   if( err == 0 ) {

      // success! get the inode number
      uint64_t file_id = fskit_core_inode_alloc( core, parent, child );
      if( file_id == 0 ) {

         fskit_error("fskit_core_inode_alloc(%s) failed\n", path );

         fskit_entry_unlock( parent );
         fskit_safe_free( path_basename );
         fskit_entry_destroy( core, child, false );
         fskit_safe_free( child );
         fskit_safe_free( path );

         return -EIO;
      }

      child->file_id = file_id;
      
      // reference, so it won't disappear
      child->open_count++;

      // perform any user-defined creations
      err = fskit_run_user_mknod( core, path, parent, child, mode, dev, cls, &inode_data );
      
      child->open_count--;
      
      if( err != 0 ) {

         // failed. abort creation
         fskit_error("fskit_run_user_mknod(%s) rc = %d\n", path, err );

         fskit_entry_unlock( parent );
         fskit_safe_free( path_basename );
         fskit_entry_destroy( core, child, true );
         fskit_safe_free( child );
         fskit_safe_free( path );

         return err;
      }

      fskit_entry_wlock( child );
      
      // attach the file
      fskit_entry_attach_lowlevel( parent, child, path_basename );

      fskit_entry_unlock( child );
   }
   else {
      fskit_error("%s(%s) rc = %d\n", method_name, path, err );
      fskit_entry_destroy( core, child, false );
      fskit_safe_free( child );
   }

   fskit_entry_unlock( parent );

   fskit_safe_free( path_basename );
   fskit_safe_free( path );

   return err;
}


// mknod, but without the user-given arg
int fskit_mknod( struct fskit_core* core, char const* fs_path, mode_t mode, dev_t dev, uint64_t user, uint64_t group ) {
   return fskit_mknod_ex( core, fs_path, mode, dev, user, group, NULL );
}

