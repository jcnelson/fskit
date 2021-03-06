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

#include <fskit/opendir.h>
#include <fskit/path.h>
#include <fskit/open.h>
#include <fskit/util.h>

#include "fskit_private/private.h"


// create a directory handle from an fskit_entry
static struct fskit_dir_handle* fskit_dir_handle_create( struct fskit_entry* dir, char const* path, void* app_handle_data ) {

   struct fskit_dir_handle* dirh = CALLOC_LIST( struct fskit_dir_handle, 1 );
   if( dirh == NULL ) {
      return NULL;
   }
   
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
// * -ENOMEM on OOM
struct fskit_dir_handle* fskit_opendir( struct fskit_core* core, char const* _path, uint64_t user, uint64_t group, int* err ) {

   void* app_handle_data = NULL;
   int rc = 0;
   struct fskit_entry* dir = NULL;
   struct fskit_dir_handle* dirh = NULL;
   char path[PATH_MAX];
   
   if( strlen(_path) >= PATH_MAX ) {
      // too long
      *err = -ENAMETOOLONG;
      return NULL;
   }

   // ensure path ends in /
   memset( path, 0, PATH_MAX );
   strncpy( path, _path, PATH_MAX - 1 );

   fskit_sanitize_path( path );

   dir = fskit_entry_resolve_path( core, path, user, group, true, err );
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

   // reference it--it cannot be unlinked 
   fskit_entry_ref_entry( dir );
   fskit_entry_unlock( dir );
   
   // generate handle data
   rc = fskit_run_user_open( core, path, dir, 0, &app_handle_data );
   
   fskit_entry_wlock( dir );
   
   if( rc != 0 ) {

      // user-run open code failed
      fskit_error("fskit_run_user_open(%s) rc = %d\n", path, rc );
      fskit_entry_unlock( dir );
      *err = rc;
      
      fskit_entry_unref( core, path, dir );
      return NULL;
   }
   
   // make a handle to it
   dirh = fskit_dir_handle_create( dir, path, app_handle_data );
   if( dirh == NULL ) {
      
      // OOM
      fskit_entry_unlock( dir );
      fskit_entry_unref( core, path, dir );
      *err = -ENOMEM;
   }

   else {
      // release the directory
      fskit_entry_unlock( dir );
   }

   return dirh;
}
