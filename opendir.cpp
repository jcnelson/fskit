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


#include "opendir.h"
#include "path.h"
#include "open.h"
#include "util.h"

// create a directory handle from an fskit_entry
static struct fskit_dir_handle* fskit_dir_handle_create( struct fskit_entry* dir, char const* path, void* app_handle_data ) {
   
   struct fskit_dir_handle* dirh = CALLOC_LIST( struct fskit_dir_handle, 1 );
   
   dirh->dent = dir;
   dirh->path = strdup( path );
   dirh->file_id = dir->file_id;
   dirh->app_data = app_handle_data;
   
   pthread_rwlock_init( &dirh->lock, NULL );
   
   return dirh;
}

 
// open a directory.
// On success, return an fskit_dir_handle
// Return any error code via the *err argument (which will be non-zero if there was an error).
// on error, return NULL, and set *err appropriately:
// * -ENAMETOOLONG if _path is too long
// * -EACCES if some part of _path is in accessible to the given user and group
// * -ENOTDIR if the entry referred to by _path isn't a directory
// * -ENOENT if the entry doesn't exist
struct fskit_dir_handle* fskit_opendir( struct fskit_core* core, char const* _path, uint64_t user, uint64_t group, int* err ) {
   
   void* app_handle_data = NULL;
   
   if( strlen(_path) >= PATH_MAX ) {
      // too long 
      *err = -ENAMETOOLONG;
      return NULL;
   }
   
   // ensure path ends in /
   char path[PATH_MAX];
   memset( path, 0, PATH_MAX );
   strncpy( path, _path, PATH_MAX - 1 );

   fskit_sanitize_path( path );
   
   struct fskit_entry* dir = fskit_entry_resolve_path( core, path, user, group, true, err );
   if( dir == NULL ) {
      // resolution error; err is set appropriately
      return NULL;
   }
   
   // make sure it's a directory
   if( dir->type != FSKIT_ENTRY_TYPE_DIR ) {
      *err = -ENOTDIR;
      fskit_entry_unlock( dir );
      return NULL;
   }
   
   // generate handle data 
   int rc = fskit_run_user_open( core, path, dir, 0, &app_handle_data );
   if( rc != 0 ) {
      
      // user-run open code failed 
      fskit_error("fskit_run_user_open(%s) rc = %d\n", path, rc );
      
      fskit_entry_unlock( dir );
      *err = rc;
      return NULL;
   }

   // open this directory
   dir->open_count++;
   
   // make a handle to it
   struct fskit_dir_handle* dirh = fskit_dir_handle_create( dir, path, app_handle_data );
   
   // release the directory
   fskit_entry_unlock( dir );
   
   return dirh;
}