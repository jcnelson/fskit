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

#include <fskit/fskit.h>
#include <fskit/open.h>
#include <fskit/path.h>
#include <fskit/utime.h>
#include <fskit/route.h>
#include <fskit/create.h>
#include <fskit/util.h>

// create a file handle from a fskit_entry
// ent must be read-locked, or otherwise un-writable
static struct fskit_file_handle* fskit_file_handle_create( struct fskit_core* core, struct fskit_entry* ent, char const* opened_path, int flags, void* handle_data ) {

   struct fskit_file_handle* fh = CALLOC_LIST( struct fskit_file_handle, 1 );

   if( fh == NULL ) {
      return NULL;
   }

   fh->fent = ent;
   fh->file_id = ent->file_id;
   fh->path = strdup( opened_path );
   fh->flags = flags;
   fh->app_data = handle_data;

   pthread_rwlock_init( &fh->lock, NULL );

   return fh;
}

// get the user-supplied handle data for opening a file
// fent *cannot* be locked
int fskit_run_user_open( struct fskit_core* core, char const* path, struct fskit_entry* fent, int flags, void** handle_data ) {

   int rc = 0;
   int cbrc = 0;
   struct fskit_route_dispatch_args dargs;

   fskit_route_open_args( &dargs, flags );

   rc = fskit_route_call_open( core, path, fent, &dargs, &cbrc );

   if( rc == -EPERM || rc == -ENOSYS ) {
      // no routes
      *handle_data = NULL;
      return 0;
   }

   else if( cbrc != 0 ) {
      // callback failed
      return cbrc;
   }

   else {
      // callback succeded
      *handle_data = dargs.handle_data;

      return 0;
   }
}


// verify that open flags are sane
static int fskit_check_flags( int flags ) {

   // NOTE: O_RDONLY | O_TRUNC is undefined by POSIX, but allowed here.

   if( (flags & O_RDONLY) == 0 && (flags & O_RDWR) != 0 && (flags & O_WRONLY) != 0 ) {
      return -EINVAL;
   }

   if( (flags & O_RDONLY) != 0 && (flags & O_WRONLY) != 0 ) {
      return -EINVAL;
   }

   return 0;
}


// verify parent status for create/open
static int fskit_do_parent_check( struct fskit_entry* parent, int flags, uint64_t user, uint64_t group ) {

   int rc = 0;

   if( parent->type != FSKIT_ENTRY_TYPE_DIR ) {

      rc = -ENOTDIR;
   }

   // can parent be searched?
   else if( !FSKIT_ENTRY_IS_DIR_SEARCHABLE( parent->mode, parent->owner, parent->group, user, group ) ) {

      rc = -EACCES;
   }

   // if we're creating, is the parent writable?
   else if( (flags & O_CREAT) && !FSKIT_ENTRY_IS_WRITEABLE(parent->mode, parent->owner, parent->group, user, group) ) {

      rc = -EACCES;
   }

   return rc;
}


// do a file open
// child must be write-locked
// on success, fill in the handle data
int fskit_do_open( struct fskit_core* core, char const* path, struct fskit_entry* child, int flags, uint64_t user, uint64_t group, void** handle_data ) {

   int rc = 0;

   // sanity check
   if( child->link_count == 0 || child->deletion_in_progress || child->type == FSKIT_ENTRY_TYPE_DEAD ) {
      rc = -ENOENT;
   }

   // sanity check
   else if( child->type == FSKIT_ENTRY_TYPE_DIR ) {
      rc = -EISDIR;
   }

   // access control for normal open
   // check read/write status of flags, and bail on error
   else if( (!(flags & O_RDWR) && !(flags & O_WRONLY))  && !FSKIT_ENTRY_IS_READABLE(child->mode, child->owner, child->group, user, group) ) {
      rc = -EACCES;  // not readable
   }
   else if( (flags & O_WRONLY) && !FSKIT_ENTRY_IS_WRITEABLE(child->mode, child->owner, child->group, user, group) ) {
      rc = -EACCES;  // not writable
   }
   else if( (flags & O_RDWR) && (!FSKIT_ENTRY_IS_READABLE(child->mode, child->owner, child->group, user, group) || !FSKIT_ENTRY_IS_WRITEABLE(child->mode, child->owner, child->group, user, group)) ) {
      rc = -EACCES;  // not readable or not writable
   }

   if( rc != 0 ) {
      // can't open
      return rc;
   }
   
   // open will succeed according to fskit.  invoke the user callback to generate handle data
   rc = fskit_run_user_open( core, path, child, flags, handle_data );
   
   if( rc != 0 ) {
      fskit_error("fskit_run_user_open(%s) rc = %d\n", path, rc );

      return rc;
   }

   else {
      // finish opening the child
      child->open_count++;

      return rc;
   }
}


// create/open a file, with the given flags and (if creating) mode
// on success, return a file handle to the created/opened file.
// on failure, return NULL and set *err to the appropriate errno
struct fskit_file_handle* fskit_open( struct fskit_core* core, char const* _path, uint64_t user, uint64_t group, int flags, mode_t mode, int* err ) {

   if( fskit_check_flags( flags ) != 0 ) {
      *err = -EINVAL;
      return NULL;
   }

   int rc = 0;

   char* path = strdup(_path);

   if( path == NULL ) {
      *err = -ENOMEM;
      return NULL;
   }

   fskit_sanitize_path( path );

   size_t basename_len = fskit_basename_len( path );
   if( basename_len > FSKIT_FILESYSTEM_NAMEMAX ) {

      free( path );
      *err = -ENAMETOOLONG;

      return NULL;
   }

   void* handle_data = NULL;

   // resolve the parent of this child (and write-lock it)
   char* path_dirname = fskit_dirname( path, NULL );
   char path_basename[FSKIT_FILESYSTEM_NAMEMAX + 1];

   memset( path_basename, 0, FSKIT_FILESYSTEM_NAMEMAX + 1 );

   fskit_basename( path, path_basename );

   struct fskit_file_handle* ret = NULL;

   struct fskit_entry* parent = fskit_entry_resolve_path( core, path_dirname, user, group, true, err );

   if( parent == NULL ) {

      fskit_safe_free( path_dirname );
      fskit_safe_free( path );

      // err is set appropriately
      return NULL;
   }

   fskit_safe_free( path_dirname );

   rc = fskit_do_parent_check( parent, flags, user, group );
   if( rc != 0 ) {

      // can't perform this operation
      fskit_entry_unlock( parent );
      fskit_safe_free( path );
      *err = rc;
      return NULL;
   }

   // resolve the child (which may be in the process of being deleted)
   struct fskit_entry* child = fskit_entry_set_find_name( parent->children, path_basename );
   bool created = false;

   if( flags & O_CREAT ) {

      if( child != NULL ) {

         fskit_entry_wlock( child );

         // it might have been marked for garbage-collection
         rc = fskit_entry_try_garbage_collect( core, path, parent, child );

         if( rc >= 0 ) {

            if( rc == 0 ) {
               // not destroyed, but no longer present
               fskit_entry_unlock( child );
            }

            // safe to create
            child = NULL;
         }
         else {

            // can't garbage-collect
            fskit_entry_unlock( parent );
            fskit_entry_unlock( child );
            fskit_safe_free( path );

            if( rc == -EEXIST ) {

               *err = -EEXIST;
               return NULL;
            }
            else {

               // shouldn't happen
               fskit_error("BUG: fskit_entry_try_garbage_collect(%s) rc = %d\n", path, rc );

               *err = -EIO;
            }

            return NULL;
         }
      }

      if( child == NULL ) {

         // can create!
         rc = fskit_do_create( core, parent, path, mode, user, group, &child, &handle_data );
         if( rc != 0 ) {

            fskit_entry_unlock( parent );
            fskit_safe_free( path );
            *err = rc;
            return NULL;
         }

         // created!
         created = true;
      }
   }

   else if( child == NULL ) {

      // not found
      fskit_entry_unlock( parent );
      fskit_safe_free( path );
      *err = -ENOENT;
      return NULL;
   }

   // now child exists.
   // don't lock it, though, since we have to pass it to truncate or open 
   
   // do we have to truncate?
   if( (flags & O_TRUNC) && (flags & (O_RDWR | O_WRONLY)) ) {

      // run user truncate
      rc = fskit_run_user_trunc( core, path, child, 0, NULL );
      if( rc != 0 ) {

         // truncate failed
         fskit_entry_unlock( parent );
         fskit_safe_free( path );
         *err = rc;
         return NULL;
      }
   }

   if( !created ) {

      // do the open
      rc = fskit_do_open( core, path, child, flags, user, group, &handle_data );
      if( rc != 0 ) {

         // open failed
         fskit_entry_unlock( parent );
         fskit_safe_free( path );
         *err = rc;
         return NULL;
      }
   }

   // still here--we can open the file now!
   fskit_entry_set_atime( child, NULL );
   ret = fskit_file_handle_create( core, child, path, flags, handle_data );

   fskit_entry_unlock( parent );

   fskit_safe_free( path );

   if( ret == NULL ) {
      // only possible if we're out of memory!
      *err = -ENOMEM;
   }

   return ret;
}
