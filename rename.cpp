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

#include "rename.h"
#include "path.h"

// check that we aren't trying to move a directory into itself
static int fskit_entry_verify_no_loop( struct fskit_entry* fent, void* cls ) {
   set<uint64_t>* file_ids = (set<uint64_t>*)cls;
   
   if( file_ids->count( fent->file_id ) > 0 ) {
      // encountered this file ID before...
      return -EINVAL;
   }
   
   file_ids->insert( fent->file_id );
   
   return 0;
}

// unlock entries
int fskit_entry_rename_unlock( struct fskit_entry* fent_common_parent, struct fskit_entry* fent_old_parent, struct fskit_entry* fent_new_parent ) {
   if( fent_old_parent ) {
      fskit_entry_unlock( fent_old_parent );
   }
   if( fent_new_parent ) {
      fskit_entry_unlock( fent_new_parent );
   }
   if( fent_common_parent ) {
      fskit_entry_unlock( fent_common_parent );
   }

   return 0;
}


int fskit_rename( struct fskit_core* core, char const* old_path, char const* new_path, uint64_t user, uint64_t group ) {
   
   int err_old = 0, err_new = 0, err = 0;
   
   // identify the parents of old_path and new_path
   char* old_path_dirname = fskit_dirname( old_path, NULL );
   char* new_path_dirname = fskit_dirname( new_path, NULL );

   struct fskit_entry* fent_old_parent = NULL;
   struct fskit_entry* fent_new_parent = NULL;
   struct fskit_entry* fent_common_parent = NULL;

   // resolve the parent *lower* in the FS hierarchy first.  order matters due to locking!
   if( fskit_depth( old_path ) > fskit_depth( new_path ) ) {
      
      fent_old_parent = fskit_entry_resolve_path( core, old_path_dirname, user, group, true, &err_old );
      if( fent_old_parent != NULL ) {
         
         set<uint64_t> file_ids;
         fent_new_parent = fskit_entry_resolve_path_cls( core, new_path_dirname, user, group, true, &err_new, fskit_entry_verify_no_loop, &file_ids );
      }
   }
   else if( fskit_depth( old_path ) < fskit_depth( new_path ) ) {
      
      set<uint64_t> file_ids;
      fent_new_parent = fskit_entry_resolve_path_cls( core, new_path_dirname, user, group, true, &err_new, fskit_entry_verify_no_loop, &file_ids );
      fent_old_parent = fskit_entry_resolve_path( core, old_path_dirname, user, group, true, &err_old );
   }
   else {
      // do these paths have the same parent?
      if( strcmp( old_path_dirname, new_path_dirname ) == 0 ) {
         // only resolve one path
         fent_common_parent = fskit_entry_resolve_path( core, old_path_dirname, user, group, true, &err_old );
      }
      else {
         // parents are different; safe to lock both
         set<uint64_t> file_ids;
         fent_new_parent = fskit_entry_resolve_path_cls( core, new_path_dirname, user, group, true, &err_new, fskit_entry_verify_no_loop, &file_ids );
         fent_old_parent = fskit_entry_resolve_path( core, old_path_dirname, user, group, true, &err_old );
      }
   }

   safe_free( old_path_dirname );
   safe_free( new_path_dirname );

   if( err_new ) {
      fskit_entry_rename_unlock( fent_common_parent, fent_old_parent, fent_new_parent );
      return err_new;
   }
   
   if( err_old ) {
      fskit_entry_rename_unlock( fent_common_parent, fent_old_parent, fent_new_parent );
      return err_old;
   }
   
   // check permission errors...
   if( (fent_new_parent != NULL && (!FSKIT_ENTRY_IS_DIR_READABLE( fent_new_parent->mode, fent_new_parent->owner, fent_new_parent->group, user, group ) ||
                                    !FSKIT_ENTRY_IS_WRITEABLE( fent_new_parent->mode, fent_new_parent->owner, fent_new_parent->group, user, group ))) ||

       (fent_old_parent != NULL && (!FSKIT_ENTRY_IS_DIR_READABLE( fent_old_parent->mode, fent_old_parent->owner, fent_old_parent->group, user, group )
                                 || !FSKIT_ENTRY_IS_WRITEABLE( fent_old_parent->mode, fent_old_parent->owner, fent_old_parent->group, user, group ))) ||

       (fent_common_parent != NULL && (!FSKIT_ENTRY_IS_DIR_READABLE( fent_common_parent->mode, fent_common_parent->owner, fent_common_parent->group, user, group )
                                    || !FSKIT_ENTRY_IS_WRITEABLE( fent_common_parent->mode, fent_common_parent->owner, fent_common_parent->group, user, group ))) ) {

      fskit_entry_rename_unlock( fent_common_parent, fent_old_parent, fent_new_parent );
      return -EACCES;
   }

   // now, look up the children
   char* new_path_basename = fskit_basename( new_path, NULL );
   char* old_path_basename = fskit_basename( old_path, NULL );

   struct fskit_entry* fent_old = NULL;
   struct fskit_entry* fent_new = NULL;

   if( fent_common_parent != NULL ) {
      fent_new = fskit_entry_set_find_name( fent_common_parent->children, new_path_basename );
      fent_old = fskit_entry_set_find_name( fent_common_parent->children, old_path_basename );
   }
   else {
      fent_new = fskit_entry_set_find_name( fent_new_parent->children, new_path_basename );
      fent_old = fskit_entry_set_find_name( fent_old_parent->children, old_path_basename );
   }

   safe_free( old_path_basename );

   // old must exist...
   err = 0;
   if( fent_old == NULL ) {
      err = -ENOENT;
   }
   
   // if we rename a file into itself, then it's okay (i.e. we're done)
   if( err != 0 || fent_old == fent_new ) {
      fskit_entry_rename_unlock( fent_common_parent, fent_old_parent, fent_new_parent );
      safe_free( new_path_basename );

      return err;
   }

   // lock the chilren
   fskit_entry_wlock( fent_old );
   if( fent_new != NULL ) {
      fskit_entry_wlock( fent_new );
   }
   
   // don't proceed if one is a directory and the other is not
   if( fent_new ) {
      if( fent_new->type != fent_old->type ) {
         if( fent_new->type == FSKIT_ENTRY_TYPE_DIR ) {
            err = -EISDIR;
         }
         else {
            err = -ENOTDIR;
         }
      }
   }
   
   if( err != 0 ) {
      
      // directory mismatch
      fskit_entry_rename_unlock( fent_common_parent, fent_old_parent, fent_new_parent );
      safe_free( new_path_basename );

      return err;
   }
   
   // perform the rename!
   struct fskit_entry* dest_parent = NULL;
   
   if( fent_common_parent != NULL ) {
      
      // dealing with entries in the same directory
      dest_parent = fent_common_parent;
      
      fskit_entry_set_remove( fent_common_parent->children, fent_old->name );

      // rename this fskit_entry
      safe_free( fent_old->name );
      fent_old->name = new_path_basename;

      if( fent_new != NULL ) {
         fskit_entry_set_remove( fent_common_parent->children, fent_new->name );
      }
      
      fskit_entry_set_insert( fent_common_parent->children, fent_old->name, fent_old );
   }
   else {
      
      // dealing with entries in different directories
      dest_parent = fent_new_parent;
      
      fskit_entry_set_remove( fent_old_parent->children, fent_old->name );

      // rename this fskit_entry
      safe_free( fent_old->name );
      fent_old->name = new_path_basename;

      if( fent_new != NULL ) {
         fskit_entry_set_remove( fent_new_parent->children, fent_new->name );
      }
      
      fskit_entry_set_insert( fent_new_parent->children, fent_old->name, fent_old );
   }
   if( fent_new ) {
      
      // clean up fent_new and erase it, since it got renamed over
      fskit_entry_unlock( fent_new );
      err = fskit_entry_detach_lowlevel( dest_parent, fent_new );
      
      if( err != 0 ) {
         // technically, it's still safe to access fent_new since dest_parent is write-locked
         errorf("fskit_entry_detach_lowlevel(%s from %s) rc = %d\n", fent_new->name, dest_parent->name, err );
      }
   }
   
   // unlock everything
   fskit_entry_rename_unlock( fent_common_parent, fent_old_parent, fent_new_parent );
   fskit_entry_unlock( fent_old );
   if( fent_new != NULL ) {
      fskit_entry_unlock( fent_new );
   }
   
   if( err != 0 ) {
      // NOTE: passed into fent_old on success, so we need to consider the error code
      safe_free( new_path_basename );
   }
   
   return err;
}
