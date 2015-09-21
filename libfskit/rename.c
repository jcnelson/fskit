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

#include <fskit/rename.h>
#include <fskit/path.h>
#include <fskit/util.h>
#include <fskit/route.h>
#include <fskit/entry.h>

#include "fskit_private/private.h"


// set of entries 
struct fskit_inode_set_entry_t {
   
   uint64_t file_id;
   
   struct fskit_inode_set_entry_t* left;
   struct fskit_inode_set_entry_t* right;
   char color;
};

typedef struct fskit_inode_set_entry_t fskit_inode_set;

#define FSKIT_INODE_SET_CMP( i1, i2 ) ((i1)->file_id < (i2)->file_id ? -1 : (i1)->file_id == (i2)->file_id ? 0 : 1)

SGLIB_DEFINE_RBTREE_PROTOTYPES( fskit_inode_set, left, right, color, FSKIT_INODE_SET_CMP );
SGLIB_DEFINE_RBTREE_FUNCTIONS( fskit_inode_set, left, right, color, FSKIT_INODE_SET_CMP );


// see if an inode set contains an inode number 
// return true if so; false if not 
static bool fskit_inode_set_contains( fskit_inode_set* inode_set, uint64_t file_id ) {
   
   fskit_inode_set* member = NULL;
   fskit_inode_set lookup;
   memset( &lookup, 0, sizeof(fskit_inode_set) );
   
   member = sglib_fskit_inode_set_find_member( inode_set, &lookup );
   if( member != NULL ) {
      
      return true;
   }
   else {
      return false;
   }
}


// callback to accumulate the set of inodes in a path.
// return 0 on success 
// return -EINVAL if a duplicate was detected (i.e. there's a path loop; indicates a bug in fskit)
// return -ENOMEM on OOM
static int fskit_entry_resolve_inodes_cb( struct fskit_entry* fent, void* cls ) {
   
   int rc = 0;
   fskit_inode_set** inode_set = (fskit_inode_set**)cls;
   
   fskit_inode_set* new_member = CALLOC_LIST( fskit_inode_set, 1 );
   if( new_member == NULL ) {
      return -ENOMEM;
   }
   
   fskit_inode_set* member = NULL;
   fskit_inode_set lookup;
   
   memset( &lookup, 0, sizeof(fskit_inode_set) );
   
   lookup.file_id = fent->file_id;
   
   member = sglib_fskit_inode_set_find_member( *inode_set, &lookup );
   if( member != NULL ) {
      
      // encountered this file ID before... 
      fskit_safe_free( new_member );
      return -EINVAL;
   }
   
   // insert 
   sglib_fskit_inode_set_add( inode_set, new_member );
   return 0;
}


// free up an inode_set
static int fskit_inode_set_free( fskit_inode_set* inode_set ) {
   
   
   struct sglib_fskit_inode_set_iterator itr;
   fskit_inode_set* dp = NULL;
   
   for( dp = sglib_fskit_inode_set_it_init_inorder( &itr, inode_set ); dp != NULL; dp = sglib_fskit_inode_set_it_next( &itr ) ) {
      
      fskit_safe_free( dp );
   }
   
   return 0;
}


// look up an entry and accumulate the set of inodes, i.e. so the caller can use it to verify that a rename won't introduce a loop.
// return a pointer to the write-locked entry on success, and sets *ret_inode_set to a malloc(3)'ed set of fskit_inode_set structures
// return NULL on failure, and set *err to:
// * -EINVAL if there was a loop 
// * -ENOMEM on OOM
// * path_resolution(7) error for things like e.g. permissions and access control
static struct fskit_entry* fskit_entry_resolve_inodes( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, int* err, fskit_inode_set** ret_inode_set ) {
   
   fskit_inode_set* inode_set = CALLOC_LIST( fskit_inode_set, 1 );
   struct fskit_entry* dirent = NULL;
   
   dirent = fskit_entry_resolve_path_cls( core, path, user, group, true, err, fskit_entry_resolve_inodes_cb, &inode_set );
   
   *ret_inode_set = inode_set;
   
   return dirent;
}


// unlock entries
static int fskit_entry_rename_unlock( struct fskit_entry* fent_common_parent, struct fskit_entry* fent_old_parent, struct fskit_entry* fent_new_parent ) {
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

// user route to rename 
// unlike all other routes on the system, this one *requres* both entries to be locked (since rename is atomic).
// old_parent, old_vent, new_parent, and dest will all be write-locked.
static int fskit_run_user_rename( struct fskit_core* core, char const* path, struct fskit_entry* old_parent, struct fskit_entry* old_fent, char const* new_path, struct fskit_entry* new_parent, struct fskit_entry* dest ) {
   
   int rc = 0;
   int cbrc = 0;
   char name[FSKIT_FILESYSTEM_NAMEMAX+1];
   
   struct fskit_route_dispatch_args dargs;
   
   memset( name, 0, FSKIT_FILESYSTEM_NAMEMAX+1 );
   fskit_basename( path, name );
   
   fskit_route_rename_args( &dargs, old_fent, name, new_path, new_parent, dest );
   
   rc = fskit_route_call_rename( core, path, old_fent, &dargs, &cbrc );
   
   if( rc == -EPERM || rc == -ENOSYS ) {
      // no routes installed
      return 0;
   }

   return cbrc;
}


// rename an inode in a directory
// return 0 on success
// NOTE: fent_common_parent must be write-locked, as must fent
// does NOT call the user route
// return -ENOMEM on OOM 
// return -ENOENT of fent is not present in fent_parent
int fskit_entry_rename_in_directory( struct fskit_entry* fent_parent, struct fskit_entry* fent, char const* old_name, char const* new_name ) {
   
   if( fskit_entry_set_find_name( fent_parent->children, old_name ) != fent ) {
      return -ENOENT;
   }
   
   char* name_dup = strdup( new_name );
   if( name_dup == NULL ) {
      return -ENOMEM;
   }
   
   fskit_entry_set_remove( &fent_parent->children, old_name );
   
   fskit_entry_set_remove( &fent_parent->children, new_name );
   fskit_entry_set_insert( &fent_parent->children, new_name, fent );
   
   return 0;
}


// rename the inode at old_path to the one at new_path. This is an atomic operation.
// return 0 on success
// return negative on failure to resolve either old_path or new_path (see path_resolution(7))
int fskit_rename( struct fskit_core* core, char const* old_path, char const* new_path, uint64_t user, uint64_t group ) {

   int err_old = 0, err_new = 0, err = 0;

   struct fskit_entry* fent_old_parent = NULL;
   struct fskit_entry* fent_new_parent = NULL;
   struct fskit_entry* fent_common_parent = NULL;
   fskit_inode_set* new_path_inodes = NULL;

   // identify the parents of old_path and new_path
   char* old_path_dirname = fskit_dirname( old_path, NULL );
   char* new_path_dirname = fskit_dirname( new_path, NULL );
   
   if( old_path_dirname == NULL || new_path_dirname == NULL ) {
      
      fskit_safe_free( old_path_dirname );
      fskit_safe_free( new_path_dirname );
      return -ENOMEM;
   }

   // resolve the parent *lower* in the FS hierarchy first.  order matters due to locking!
   if( fskit_depth( old_path ) > fskit_depth( new_path ) ) {

      fent_old_parent = fskit_entry_resolve_path( core, old_path_dirname, user, group, true, &err_old );
      if( fent_old_parent != NULL ) {

         fent_new_parent = fskit_entry_resolve_inodes( core, new_path_dirname, user, group, &err_new, &new_path_inodes );
      }
   }
   else if( fskit_depth( old_path ) < fskit_depth( new_path ) ) {

      fent_new_parent = fskit_entry_resolve_inodes( core, new_path_dirname, user, group, &err_new, &new_path_inodes );
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
         fent_new_parent = fskit_entry_resolve_inodes( core, new_path_dirname, user, group, &err_new, &new_path_inodes );
         fent_old_parent = fskit_entry_resolve_path( core, old_path_dirname, user, group, true, &err_old );
      }
   }

   fskit_safe_free( old_path_dirname );
   fskit_safe_free( new_path_dirname );

   if( err_new ) {
      
      fskit_entry_rename_unlock( fent_common_parent, fent_old_parent, fent_new_parent );
      
      if( new_path_inodes != NULL ) {
         fskit_inode_set_free( new_path_inodes );
      }
      
      return err_new;
   }

   if( err_old ) {
      
      fskit_entry_rename_unlock( fent_common_parent, fent_old_parent, fent_new_parent );
      
      if( new_path_inodes != NULL ) {
         fskit_inode_set_free( new_path_inodes );
      }
      
      return err_old;
   }

   // check permission errors...
   if( (fent_new_parent != NULL && (!FSKIT_ENTRY_IS_DIR_SEARCHABLE( fent_new_parent->mode, fent_new_parent->owner, fent_new_parent->group, user, group ) ||
                                    !FSKIT_ENTRY_IS_WRITEABLE( fent_new_parent->mode, fent_new_parent->owner, fent_new_parent->group, user, group ))) ||

       (fent_old_parent != NULL && (!FSKIT_ENTRY_IS_DIR_SEARCHABLE( fent_old_parent->mode, fent_old_parent->owner, fent_old_parent->group, user, group )
                                 || !FSKIT_ENTRY_IS_WRITEABLE( fent_old_parent->mode, fent_old_parent->owner, fent_old_parent->group, user, group ))) ||

       (fent_common_parent != NULL && (!FSKIT_ENTRY_IS_DIR_SEARCHABLE( fent_common_parent->mode, fent_common_parent->owner, fent_common_parent->group, user, group )
                                    || !FSKIT_ENTRY_IS_WRITEABLE( fent_common_parent->mode, fent_common_parent->owner, fent_common_parent->group, user, group ))) ) {

      fskit_entry_rename_unlock( fent_common_parent, fent_old_parent, fent_new_parent );
      
      if( new_path_inodes != NULL ) {
         fskit_inode_set_free( new_path_inodes );
      }
      
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

   fskit_safe_free( old_path_basename );

   // old must exist...
   err = 0;
   if( fent_old == NULL ) {
      err = -ENOENT;
   }

   // if we rename a file into itself, then it's okay (i.e. we're done)
   if( err != 0 || fent_old == fent_new ) {
      
      fskit_entry_rename_unlock( fent_common_parent, fent_old_parent, fent_new_parent );
      
      fskit_safe_free( new_path_basename );
      
      if( new_path_inodes != NULL ) {
         fskit_inode_set_free( new_path_inodes );
      }
      
      return err;
   }

   // lock the chilren, so we can check for EISDIR and ENOTDIR
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
   
   if( err == 0 ) {
      
      // verify no loops 
      if( new_path_inodes != NULL && fskit_inode_set_contains( new_path_inodes, fent_new->file_id ) ) {
         
         // this rename would introduce a loop
         err = -EINVAL;
      }
   }
   
   if( err != 0 ) {

      // directory mismatch, or loop 
      fskit_entry_rename_unlock( fent_common_parent, fent_old_parent, fent_new_parent );
      
      fskit_safe_free( new_path_basename );
      
      if( new_path_inodes != NULL ) {
         fskit_inode_set_free( new_path_inodes );
      }

      return err;
   }
   
   if( new_path_inodes != NULL ) {
      fskit_inode_set_free( new_path_inodes );
      new_path_inodes = NULL;
   }
   
   // user rename...
   // note that by construction, the consistency discipline will *not* be FSKIT_INODE_SEQUENTIAL.
   // this means it's safe to lock these entries.
   if( fent_common_parent != NULL ) {
      
      err = fskit_run_user_rename( core, old_path, fent_common_parent, fent_old, new_path, fent_common_parent, fent_new );
   }
   else {
      
      err = fskit_run_user_rename( core, old_path, fent_old_parent, fent_old, new_path, fent_new_parent, fent_new );
   }
   
   if( err != 0 ) {
      
      fskit_entry_unlock( fent_old );
      if( fent_new != NULL ) {
         fskit_entry_unlock( fent_new );
      }
      
      // user rename failure 
      fskit_entry_rename_unlock( fent_common_parent, fent_old_parent, fent_new_parent );
      fskit_safe_free( new_path_basename );
      
      return err;
   }

   // perform the rename!
   struct fskit_entry* dest_parent = NULL;
   err = 0;

   if( fent_common_parent != NULL ) {

      // dealing with entries in the same directory
      dest_parent = fent_common_parent;
      
      fskit_entry_detach_lowlevel( fent_common_parent, old_path_basename );
      
      // rename this fskit_entry
      if( fent_new != NULL ) {
         fskit_entry_detach_lowlevel( fent_common_parent, new_path_basename );
      }

      fskit_entry_attach_lowlevel( fent_common_parent, fent_old, new_path_basename );
      fskit_safe_free( new_path_basename );
   }
   else {

      // dealing with entries in different directories
      dest_parent = fent_new_parent;

      fskit_entry_detach_lowlevel( fent_old_parent, old_path_basename );

      // rename this fskit_entry
      if( fent_new != NULL ) {
         fskit_entry_detach_lowlevel( fent_new_parent, new_path_basename );
      }

      fskit_entry_attach_lowlevel( fent_new_parent, fent_old, new_path_basename );
      fskit_safe_free( new_path_basename );
   }
   
   fskit_entry_unlock( fent_old );
   
   if( fent_new != NULL && err == 0 ) {

      // unref up fent_new (the overwritten inode) 
      fskit_entry_unlock( fent_new );
      
      if( fent_common_parent != NULL ) {
         err = fskit_entry_try_destroy_and_free( core, new_path, fent_common_parent, fent_new );
      }
      else {
         err = fskit_entry_try_destroy_and_free( core, new_path, fent_new_parent, fent_new );
      }
      
      if( err == 0 ) {
         
         // not destroyed 
         fskit_entry_unlock( fent_new );
      }
   }
   
   // unlock everything
   fskit_entry_rename_unlock( fent_common_parent, fent_old_parent, fent_new_parent );
   
   if( err != 0 ) {
      // NOTE: passed into fent_old on success, so we need to consider the error code
      fskit_safe_free( new_path_basename );
   }

   return err;
}
