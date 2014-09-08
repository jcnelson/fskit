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

#include "open.h"
#include "path.h"
#include "utime.h"

// create a file handle from a fskit_entry
// ent must be read-locked
static struct fskit_file_handle* fskit_file_handle_create( struct fskit_core* core, struct fskit_entry* ent, char const* opened_path, int flags ) {
   
   struct fskit_file_handle* fh = CALLOC_LIST( struct fskit_file_handle, 1 );
   
   fh->fent = ent;
   fh->file_id = ent->file_id;
   fh->path = strdup( opened_path );
   fh->flags = flags;
   
   pthread_rwlock_init( &fh->lock, NULL );
   
   return fh;
}

// create/open a file, with the given flags and (if creating) mode
// on success, return a file handle to the created/opened file.
// on failure, return NULL and set *err to the appropriate errno
struct fskit_file_handle* fskit_open( struct fskit_core* core, char const* _path, uint64_t user, uint64_t group, int flags, mode_t mode, void* app_data, int* err ) {

   int rc = 0;
   
   char* path = strdup(_path);
   fskit_sanitize_path( path );
   
   
   // resolve the parent of this child (and write-lock it)
   char* path_dirname = fskit_dirname( path, NULL );
   char* path_basename = fskit_basename( path, NULL );
   struct fskit_file_handle* ret = NULL;

   struct fskit_entry* parent = fskit_entry_resolve_path( core, path_dirname, user, group, true, err );

   if( parent == NULL ) {

      free( path_basename );
      free( path_dirname );
      free( path );

      // err is set appropriately
      return NULL;
   }

   free( path_dirname );

   if( parent->type != FSKIT_ENTRY_TYPE_DIR ) {
      // parent is not a directory
      fskit_entry_unlock( parent );
      free( path_basename );
      free( path );
      *err = -ENOTDIR;
      return NULL;
   }

   *err = 0;

   // can parent be searched?
   if( !FSKIT_ENTRY_IS_DIR_READABLE( parent->mode, parent->owner, parent->group, user, group ) ) {
      // nope
      fskit_entry_unlock( parent );
      free( path_basename );
      free( path );
      *err = -EACCES;
      return NULL;
   }
   // resolve the child (which may be in the process of being deleted)
   struct fskit_entry* child = fskit_entry_set_find_name( parent->children, path_basename );
   bool created = false;

   if( flags & O_CREAT ) {
      if( child != NULL ) {
         // can't create
         fskit_entry_unlock( parent );
         free( path_basename );
         free( path );
         *err = -ENOENT;
         return NULL;
      }
      else if( !FSKIT_ENTRY_IS_WRITEABLE(parent->mode, parent->owner, parent->group, user, group) ) {
         // can't create
         fskit_entry_unlock( parent );
         free( path_basename );
         free( path );
         *err = -EACCES;
         return NULL;
      }
      else {

         // can create--initialize the child
         child = CALLOC_LIST( struct fskit_entry, 1 );
         
         rc = fskit_entry_init_file( child, 0, path_basename, user, group, mode, app_data );

         if( rc != 0 ) {
            errorf("fskit_entry_init_file(%s) rc = %d\n", path, rc );
            *err = rc;

            fskit_entry_unlock( parent );
            free( path_basename );
            free( path );
            fskit_entry_destroy( child, false, NULL );
            free( child );
            
            return NULL;
         }
         else {
            
            // get an inode for this file 
            uint64_t child_inode = fskit_core_inode_alloc( core, parent, child );
            if( child_inode == 0 ) {
               // error in allocating 
               errorf("fskit_core_inode_alloc(%s) failed\n", path );
               
               *err = -EIO;
                  
               fskit_entry_unlock( parent );
               free( path_basename );
               free( path );
               fskit_entry_destroy( child, false, NULL );
               free( child );
               
               return NULL;
            }
            
            // insert it into the filesystem
            fskit_entry_wlock( child );
            
            child->file_id = child_inode;
            
            // open it
            child->open_count++;
            
            fskit_entry_attach_lowlevel( parent, child );
            created = true;
            
            fskit_entry_unlock( child );
            
         }
      }
   }
   else if( child == NULL || child->deletion_in_progress ) {
      // not found
      fskit_entry_unlock( parent );
      free( path_basename );
      free( path );
      *err = -ENOENT;
      return NULL;
   }

   // now child exists.
   
   // safe to lock it so we can release the parent
   fskit_entry_wlock( child );
   fskit_entry_unlock( parent );

   if( child->link_count <= 0 ) {
      // only possible if we didn't just create
      // someone unlinked this child at the last minute
      // can't open
      fskit_entry_unlock( child );
      free( path_basename );
      free( path );
      *err = -ENOENT;
      return NULL;
   }

   if( child->type != FSKIT_ENTRY_TYPE_DIR ) {
      // only possible if we didn't just create
      // can't open
      fskit_entry_unlock( child );
      free( path_basename );
      free( path );
      *err = -EISDIR;
      return NULL;
   }

   if( !created ) {

      // access control
      // check read/write status of flags, and bail on error
      if( (!(flags & O_RDWR) && !(flags & O_WRONLY))  && !FSKIT_ENTRY_IS_READABLE(child->mode, child->owner, child->group, user, group) ) {
         *err = -EACCES;  // not readable
      }
      else if( (flags & O_WRONLY) && !FSKIT_ENTRY_IS_WRITEABLE(child->mode, child->owner, child->group, user, group) ) {
         *err = -EACCES;  // not writable
      }
      else if( (flags & O_RDWR) && (!FSKIT_ENTRY_IS_READABLE(child->mode, child->owner, child->group, user, group) || !FSKIT_ENTRY_IS_WRITEABLE(child->mode, child->owner, child->group, user, group)) ) {
         *err = -EACCES;  // not readable or not writable
      }
      if( *err != 0 ) {
         // can't do I/O
         fskit_entry_unlock( child );
         free( path_basename );
         free( path );
         return NULL;
      }
      
      // finish opening the child
      child->open_count++;
   }
   
   if( *err == 0 ) {
      // still here--we can open the file now!
      fskit_entry_set_atime( child, NULL );
      ret = fskit_file_handle_create( core, child, path, flags );
   }
   
   if( child ) {
      // merely opened
      fskit_entry_unlock( child );
   }
   
   free( path_basename );
   free( path );
   
   return ret;
}
