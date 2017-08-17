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


#include "fskit_private/private.h"

#include <fskit/debug.h>
#include <fskit/entry.h>
#include <fskit/path.h>
#include <fskit/random.h>
#include <fskit/route.h>
#include <fskit/util.h>

struct fskit_entry_set_entry {
   
   char* name;
   struct fskit_entry* dirent;
   
   struct fskit_entry_set_entry* left;
   struct fskit_entry_set_entry* right;
   char color;
};

// linked list entry for destroying an entry and all of its children
struct fskit_detach_entry {
   
   char* path;
   struct fskit_entry* ent;
   
   struct fskit_detach_entry* next;
};

struct fskit_detach_ctx {
   
   struct fskit_detach_entry* head;
   struct fskit_detach_entry* tail;
   size_t size;

   int flags;
   int cbrc;
};

struct fskit_inode_metadata {
   uint64_t fields;

   mode_t mode;
   uint64_t owner;
   uint64_t group;
};

// prototypes...
int fskit_run_user_destroy( struct fskit_core* core, char const* path, struct fskit_entry* parent, struct fskit_entry* fent );

SGLIB_DEFINE_RBTREE_FUNCTIONS( fskit_entry_set, left, right, color, FSKIT_ENTRY_SET_ENTRY_CMP );

SGLIB_DEFINE_RBTREE_FUNCTIONS( fskit_xattr_set, left, right, color, FSKIT_XATTR_SET_ENTRY_CMP );

// start iterating over a set of directory entries 
fskit_entry_set* fskit_entry_set_begin( fskit_entry_set_itr* itr, fskit_entry_set* dirents ) {
   
   return sglib_fskit_entry_set_it_init_inorder( itr, dirents );
}

// get the next entry in a directory entry set 
fskit_entry_set* fskit_entry_set_next( fskit_entry_set_itr* itr ) {
   
   return sglib_fskit_entry_set_it_next( itr );
}

// free up all entries in an fskit_entry_set, as well as the entry set itself.
// don't free the contained entries
int fskit_entry_set_free( fskit_entry_set* dirents ) {
   
   if( dirents == NULL ) {
       return 0;
   }
   
   fskit_entry_set_itr itr;   
   fskit_entry_set* dp = NULL;
   fskit_entry_set* old_dp = NULL;

   for( dp = fskit_entry_set_begin( &itr, dirents ); dp != NULL; ) {
      
      fskit_safe_free( dp->name );
      dp->dirent = NULL;
      
      old_dp = dp;
      dp = fskit_entry_set_next( &itr );
      fskit_safe_free( old_dp );
   }
   
   return 0;
}

// allocate and initialize an empty fskit_entry_set
// return the set on success
// return NULL on error (OOM)
fskit_entry_set* fskit_entry_set_new( struct fskit_entry* node, struct fskit_entry* parent ) {

   int rc = 0;
   fskit_entry_set* ret = CALLOC_LIST( fskit_entry_set, 1 );
   
   if( ret == NULL ) {
      return NULL;
   }
   
   char* name_dup = strdup_or_null( "." );
   if( name_dup == NULL ) {
       fskit_safe_free( ret );
       return NULL;
   }
   
   ret->name = name_dup;
   ret->dirent = node;
   
   rc = fskit_entry_set_insert( &ret, "..", parent );
   if( rc != 0 ) {
      
      fskit_entry_set_free( ret );
      return NULL;
   }
   
   return ret;
}

// insert a child entry into an fskit_entry_set
// return 0 on success
// return -ENOMEM on OOM
int fskit_entry_set_insert( fskit_entry_set** set, char const* name, struct fskit_entry* child ) {
   
   fskit_entry_set* new_entry = NULL;
   char* name_dup = NULL;
   int rc = 0;
   
   new_entry = CALLOC_LIST( fskit_entry_set, 1 );
   if( new_entry == NULL ) {
      return -ENOMEM;
   }
   
   name_dup = strdup_or_null( name );
   if( name_dup == NULL ) {
      
      fskit_safe_free( new_entry );
      return -ENOMEM;
   }
   
   new_entry->name = name_dup;
   new_entry->dirent = child;
   
   sglib_fskit_entry_set_add( set, new_entry );
   
   return 0;
}


// find a child entry set in a fskit_entry_set
// return NULL if not found 
fskit_entry_set* fskit_entry_set_find_itr( fskit_entry_set* set, char const* name ) {
    
   fskit_entry_set* member = NULL;
   fskit_entry_set lookup;
   
   memset( &lookup, 0, sizeof( fskit_entry_set ) );
   lookup.name = (char*)name;
   
   return sglib_fskit_entry_set_find_member( set, &lookup );
}


// find a child entry in a fskit_entry_set
// return NULL if not found
struct fskit_entry* fskit_entry_set_find_name( fskit_entry_set* set, char const* name ) {
   
   fskit_entry_set* member = fskit_entry_set_find_itr( set, name );
   
   if( member == NULL ) {
      return NULL;
   }
   else {
      return member->dirent;
   }
}


// remove a child entry from an fskit_entry_set.  Note that it does *NOT* free the fskit_entry contained within.
// return true if removed; false if not
bool fskit_entry_set_remove( fskit_entry_set** set, char const* name ) {
   
   int rc = 0;
   int size = 0;
   fskit_entry_set lookup;
   fskit_entry_set* member = NULL;
   
   // cannot remove . or .. 
   if( strcmp(name, ".") == 0 || strcmp(name, "..") == 0 ) {
       
      fskit_error("BUG: tried to remove '%s'\n", name);
      return false;
   }
   
   memset( &lookup, 0, sizeof( fskit_entry_set ) );
   lookup.name = (char*)name;
   
   sglib_fskit_entry_set_delete_if_member( set, &lookup, &member );
   if( member != NULL ) {
      
      fskit_safe_free( member->name );
      fskit_safe_free( member );
      
      return true;
   }
   else {
      return false;
   }
}


// replace an existing entry
// return true if replaced; false if not
bool fskit_entry_set_replace( fskit_entry_set* set, char const* name, struct fskit_entry* replacement ) {
   
   int rc = 0;
   fskit_entry_set* member = NULL;
   fskit_entry_set lookup;
   
   memset( &lookup, 0, sizeof( fskit_entry_set ) );
   lookup.name = (char*)name;
   
   member = sglib_fskit_entry_set_find_member( set, &lookup );
   if( member != NULL ) {
      
      member->dirent = replacement;
      return true;
   }
   else {
      
      return false;
   }
}


// count the number of slots available in an fskit_entry_set
// not to be confused with the number of children, which is the number of non-NULL slots
unsigned int fskit_entry_set_count( fskit_entry_set* set ) {
   
   return sglib_fskit_entry_set_len( set );
}

// get the child (or NULL if the request is off the end of the set)
struct fskit_entry* fskit_entry_set_child_at( fskit_entry_set* dp ) {

   if( dp == NULL ) {
      return NULL;
   }
   else {
      return dp->dirent;
   }
}

// get the child's name (or NULL if it's off the end of the set)
char const* fskit_entry_set_name_at( fskit_entry_set* dp ) {

   if( dp == NULL ) {
      return NULL;
   }
   else {
      return dp->name;
   }
}


// find a child by name.
// dir must be at least read-locked
// return NULL if not found, or if not a directory
// NOTE: dir must be write-locked
struct fskit_entry* fskit_dir_find_by_name( struct fskit_entry* dir, char const* name ) {

   if( dir->children == NULL ) {
      return NULL;
   }

   return fskit_entry_set_find_name( dir->children, name );
}


// attach an entry as a child directly
// both fskit_entry structures must be write-locked
// return 0 on success 
// return -ENOMEM on OOM
// NOTE: parent must be write-locked, as well as fent
int fskit_entry_attach_lowlevel( struct fskit_entry* parent, struct fskit_entry* fent, char const* name ) {

   if( parent != fent ) {
      fent->link_count++;
   }
   
   parent->num_children++;

   struct timespec ts;
   clock_gettime( CLOCK_REALTIME, &ts );
   parent->mtime_sec = ts.tv_sec;
   parent->mtime_nsec = ts.tv_nsec;
   
   // if this is a directory, then set .. to point to the parent 
   if( fent->type == FSKIT_ENTRY_TYPE_DIR ) {
       
       fskit_entry_set_replace( fent->children, "..", parent );
   }

   return fskit_entry_set_insert( &parent->children, name, fent );
}


// detach an entry from a parent.
// both entries must be write-locked.
// child's link count will be decremented
// parent must be write-locked
// child must be write-locked, or otherwise inaccessible
// the child will not be destroyed even if its link count reaches zero; the caller must take care of that.
static int fskit_entry_detach_lowlevel_ex( struct fskit_entry* parent, char const* child_name, bool update_mtime ) {

   struct fskit_entry* child = fskit_entry_set_find_name( parent->children, child_name );
   if( child == NULL ) {
      
      fskit_error("fskit_entry_set_find_name(%p, '%s') == NULL\n", parent, child_name );
      return -ENOENT;
   }
   
   if( parent == child ) {
      // tried to detach .
      return -ENOTEMPTY;
   }
   
   // if the child is a directory, and it's not empty, then don't proceed
   if( child->type == FSKIT_ENTRY_TYPE_DIR && fskit_entry_get_num_children( child ) > 2 ) {
      // not empty
      return -ENOTEMPTY;
   }

   // unlink
   bool rc = fskit_entry_set_remove( &parent->children, child_name );
   if( !rc ) {
      
      fskit_error("fskit_entry_set_remove(%" PRIX64 ", '%s') rc = false\n", parent->file_id, child_name );
      return -ENOENT;
   }
   
   struct timespec ts;
   clock_gettime( CLOCK_REALTIME, &ts );
   parent->mtime_sec = ts.tv_sec;
   parent->mtime_nsec = ts.tv_nsec;
   parent->num_children--;

   if( parent != child ) {
      
      child->link_count--;

      // should *never* happen
      if( child->link_count < 0 ) {
         fskit_error("BUG: negative link count on %" PRIX64 " ('%s')\n", child->file_id, child_name );
         child->link_count = 0;
      }
   }

   return 0;
}


// detach an entry from a parent
// both entries must be write-locked.
// child's link count will be decremented
// parent must be write-locked, so it won't matter if the child is not.
// the child will not be destroyed even if its link count reaches zero; the caller must take care of that.
int fskit_entry_detach_lowlevel( struct fskit_entry* parent, char const* child_name ) {
   return fskit_entry_detach_lowlevel_ex( parent, child_name, true );
}

// default inode allocator: pick a random 64-bit number
static uint64_t fskit_default_inode_alloc( struct fskit_entry* parent, struct fskit_entry* child_to_receive_inode, void* ignored ) {
   uint64_t upper = fskit_random32();
   uint64_t lower = fskit_random32();

   return (upper << 32) | lower;
}

// default inode releaser; does nothing
static int fskit_default_inode_free( uint64_t inode, void* ignored ) {
   return 0;
}

// create a filesystem core 
// return a pointer to a malloc'ed core on success 
// return NULL on OOM
struct fskit_core* fskit_core_new() {
   return CALLOC_LIST( struct fskit_core, 1 );
}

// get the root inode 
struct fskit_entry* fskit_core_get_root( struct fskit_core* core ) {
   return &core->root;
}

// initialize the fskit core
// return 0 on success 
// return -ENOMEM on OOM
int fskit_core_init( struct fskit_core* core, void* app_fs_data ) {

   int rc = 0;

   fskit_route_table* routes = fskit_route_table_new();
   if( routes == NULL ) {
      return -ENOMEM;
   }

   rc = fskit_entry_init_dir( &core->root, &core->root, 0, 0, 0, 0755 );
   if( rc != 0 ) {
      fskit_error("fskit_entry_init_dir(/) rc = %d\n", rc );

      fskit_safe_free( routes );
      return rc;
   }

   core->root.link_count = 1;
   core->app_fs_data = app_fs_data;

   core->fskit_inode_alloc = fskit_default_inode_alloc;
   core->fskit_inode_free = fskit_default_inode_free;

   core->routes = routes;

   pthread_rwlock_init( &core->lock, NULL );
   pthread_rwlock_init( &core->route_lock, NULL );

   return 0;
}


// destroy the fskit core
// core must be write-locked
// return the app_fs_data from core upon distruction
int fskit_core_destroy( struct fskit_core* core, void** app_fs_data ) {

   void* fs_data = NULL;
   
   fskit_entry_wlock( &core->root );
   
   // forcibly detach core->root 
   core->root.open_count = 1;   // for referencing
   core->root.link_count = 0;
   core->root.deletion_in_progress = true;
   
   fskit_entry_unlock( &core->root );
   
   fskit_debug("%s", "Destroy root inode\n");
   fskit_run_user_destroy( core, "/", NULL, &core->root );
   
   fskit_entry_destroy( core, &core->root, true );

   fskit_route_table_free( core->routes );
   
   fs_data = core->app_fs_data;
   core->app_fs_data = NULL;

   pthread_rwlock_destroy( &core->lock );
   pthread_rwlock_destroy( &core->route_lock );

   if( app_fs_data != NULL ) {
      *app_fs_data = fs_data;
   }
   
   memset( core, 0, sizeof(struct fskit_core) );

   return 0;
}


// queue a child for detach.  It must have been detached from a parent already (in fskit_detach_all_ex), but it may have children of its own.
// NOTE: the child is not guaranteed to be locked
// return 0 on success
// return -ENOMEM on OOM
// return -EPERM if the child is not fully unlinked
int fskit_detach_queue_child( struct fskit_detach_ctx* ctx, char const* dir_path, char const* name, struct fskit_entry* child ) {

   struct fskit_detach_entry* next = NULL;
   char* child_path = NULL;
   
   child_path = fskit_fullpath( dir_path, name, NULL );
   if( child_path == NULL ) {

      return -ENOMEM;
   }
   
   next = CALLOC_LIST( struct fskit_detach_entry, 1 );
   if( next == NULL ) {
      
      fskit_safe_free( child_path );
      return -ENOMEM;
   }
   
   // enqueue...
   next->path = child_path;
   next->ent = child;
   next->next = NULL;
   
   if( ctx->head == NULL ) {
      ctx->head = next;
      ctx->tail = next;
   }
   else {
      ctx->tail->next = next;
      ctx->tail = next;
   }
   
   ctx->size++;
   
   return 0;
}


// queue a set of children for detaching.
// the *dir_children set must refer to a set of entries that have just been removed from their parent (but not been modified themselves).
// consume all but . and .. from dir_children
// return 0 on success
// return -ENOMEM if out of memory
int fskit_detach_queue_children( struct fskit_detach_ctx* ctx, char const* dir_path, fskit_entry_set** dir_children ) {

   int rc = 0;
   fskit_entry_set_itr itr;
   fskit_entry_set* dirent = NULL;
   fskit_entry_set* children = NULL;
    
   struct fskit_detach_entry* erased = ctx->tail;
   struct fskit_detach_entry* tmp = NULL;
   
   char erased_name[FSKIT_FILESYSTEM_NAMEMAX+1];
   
   for( dirent = fskit_entry_set_begin( &itr, *dir_children ); dirent != NULL; dirent = fskit_entry_set_next( &itr ) ) {
    
      struct fskit_entry* child = fskit_entry_set_child_at( dirent );
      char const* name = fskit_entry_set_name_at( dirent );
      
      if( strcmp( name, "." ) == 0 || strcmp( name, ".." ) == 0 ) {
         
         // skip
         continue;
      }
      
      if( child == NULL ) {
         
         // should never happen 
         fskit_error("BUG: null child at %p in '%s'\n", dirent, dir_path );
         continue;
      }
      
      if( child->type == FSKIT_ENTRY_TYPE_DEAD ) {
         
         // should never happen 
         fskit_error("BUG: dead child at %p in '%s'\n", dirent, dir_path );
         continue;
      }
      
      rc = fskit_detach_queue_child( ctx, dir_path, name, child );

      if( rc != 0 ) {
         return rc;
      }
   }
   
   // go through all the children we enqueued and remove them from the dir_children set
   for( tmp = erased; tmp != NULL; tmp = tmp->next ) {
      
      fskit_dirname( tmp->path, erased_name );
      
      fskit_entry_set_remove( dir_children, erased_name );
   }

   return 0;
}


// unlink a directory's immediate children and subsequent descendants.
// *dir_children must be the directory's old set of children; the directory must have been given a new set of children in which none of these children are present.
// (e.g. this is a "mass-unlink" function that takes care of updating all the children).
// run any detach route callbacks if their link counts reach 0.
// destroy the contents of dir_children for which the entries have been fully unlinked (besides . and ..).
// return 0 on success
// return -ENOMEM if out of memory
// return -EFAULT if the FSKIT_DETACH_CTX_CB_FAIL flag is set, and the callback fails.
// NOTE: the owner of dir_children should be write-locked
// NOTE: if -ENOMEM is encountered, this method will fail fast and return.
// This is because it will be unable to safely run user-defined routes.
// If this occurs, free up some memory and call this method again with the same detach context, but NULL for dir_children
int fskit_detach_all_ex( struct fskit_core* core, char const* dir_path, fskit_entry_set** dir_children, struct fskit_detach_ctx* ctx ) {

   // NOTE: it is important that we go in breadth-first order.  This is because fskit
   // locks the parent before the child when resolving a path.  So it must be the case
   // here to avoid deadlock.

   int rc = 0;
   int cbrc = 0;

   // queue immediate children for destruction
   if( dir_children != NULL ) {

      rc = fskit_detach_queue_children( ctx, dir_path, dir_children );
      if( rc != 0 ) {
         // OOM
         return rc;
      }
   }

   while( ctx->size > 0 && rc == 0 ) {

      // reap unlinked children
      struct fskit_detach_entry* next = ctx->head;
      struct fskit_entry* fent = next->ent;
      char* fent_path = next->path;
      
      fskit_entry_wlock( fent );

      if( fent->type == FSKIT_ENTRY_TYPE_DIR ) {

         // garbage-collect: detach children from their parent, and mark them as garbage
         fskit_entry_set* children = NULL;
         rc = fskit_entry_tag_garbage( fent, &children );
         if( rc != 0 ) {
            
            fskit_error("fskit_entry_tag_garbage('%" PRIX64 "') rc = %d\n", fent->file_id, rc);
            fskit_entry_unlock( fent );
            return rc;
         }
         
         if( children != NULL ) {
             
            // add children into the unlink queue
            rc = fskit_detach_queue_children( ctx, fent_path, &children );
            if( rc != 0 ) {
                
               fskit_error("fskit_detach_queue_children('%s') rc = %d\n", fent_path, rc );
               fskit_entry_unlock( fent );
               return rc;
            }
            
            fskit_entry_set_free( children );
            children = NULL;
         }
      }
      
      // mark this entry for garbage-collection.
      // it was detached from exactly one parent by this method.
      fent->link_count--;
      
      if( fent->type == FSKIT_ENTRY_TYPE_DIR ) {
         fent->deletion_in_progress = true;
      }
      
      // maybe this entry is fully unref'ed...
      rc = fskit_entry_try_destroy_and_free_ex( core, fent_path, NULL, fent, &cbrc );
      if( (ctx->flags & FSKIT_DETACH_CTX_CB_FAIL) && cbrc < 0 ) {
         fskit_error("Callback failed (rc = %d)\n", cbrc );
         fskit_entry_unlock( fent );
         ctx->cbrc = cbrc;
         return -EFAULT;
      }
      if( rc >= 0 ) {

         if( rc == 0 ) {
            // not destroyed--still opened somewhere
            fskit_entry_unlock( fent );
         }
         else {
            // destroyed
            rc = 0;
         }
      }
      else {
         // shouldn't happen: failed to destroy and free
         fskit_error("BUG: fskit_entry_try_destroy_and_free(%s) rc = %d\n", fent_path, rc );
         
         fskit_entry_unlock( fent );
         return rc;
      }

      // consumed!
      ctx->head = ctx->head->next;
      ctx->size--;
      
      fskit_safe_free( next->path );
      fskit_safe_free( next );
   }

   // if all went well, then ctx's queues will be empty
   return rc;
}


// create a detach context 
struct fskit_detach_ctx* fskit_detach_ctx_new() {
   return CALLOC_LIST( struct fskit_detach_ctx, 1 );
}

// set up a detach context
int fskit_detach_ctx_init( struct fskit_detach_ctx* ctx ) {

   memset( ctx, 0, sizeof(struct fskit_detach_ctx) );
   return 0;
}

// set a flag 
int fskit_detach_ctx_set_flags( struct fskit_detach_ctx* ctx, int flags ) {

   int old = ctx->flags;
   ctx->flags = flags;
   return old;
}

// get the last callback return code 
int fskit_detach_ctx_get_cbrc( struct fskit_detach_ctx* ctx ) {
   return ctx->cbrc;
}

// free a detach context
int fskit_detach_ctx_free( struct fskit_detach_ctx* ctx ) {

   struct fskit_detach_entry* to_erase = NULL;
   struct fskit_detach_entry* tmp = NULL;
   
   for( to_erase = ctx->head; to_erase != NULL; ) {
      
      tmp = to_erase;
      to_erase = to_erase->next;
      
      fskit_safe_free( tmp->path );
      fskit_safe_free( tmp );
   }
   
   ctx->head = NULL;
   ctx->tail = NULL;

   return 0;
}

// if you expect that fskit_detach_all_ex will succeed in one go, then you can use this helper function
// remove all entries below a given path.  clear out the directory at root_path
int fskit_detach_all( struct fskit_core* core, char const* root_path ) {

   struct fskit_detach_ctx ctx;
   fskit_entry_set* dir_children = NULL;
   int rc = 0;
   struct fskit_entry* dent = NULL;

   rc = fskit_detach_ctx_init( &ctx );
   if( rc != 0 ) {
      return rc;
   }
   
   dent = fskit_entry_resolve_path( core, root_path, 0, 0, true, &rc );
   if( dent == NULL ) {
       fskit_detach_ctx_free( &ctx );
       return rc;
   }
   
   // swap out the children, and mark this directory as garbage-collectable
   rc = fskit_entry_tag_garbage( dent, &dir_children );
   if( rc != 0 ) {
       fskit_detach_ctx_free( &ctx );
       fskit_error("fskit_entry_tag_garbage('%" PRIX64 "') rc = %d\n", dent->file_id, rc );
       fskit_entry_unlock( dent );
       return rc;
   }
   
   fskit_entry_unlock( dent );

   // proceed to detach
   while( true ) {
      rc = fskit_detach_all_ex( core, root_path, &dir_children, &ctx );
      if( rc == 0 ) {
         break;
      }
      else if( rc == -ENOMEM ) {
         continue;
      }
      else {
         break;
      }
   }
   
   fskit_entry_set_free( dir_children );

   fskit_detach_ctx_free( &ctx );

   return rc;
}

// set the inode allocator
int fskit_core_inode_alloc_cb( struct fskit_core* core, fskit_inode_alloc_t inode_alloc ) {

   int rc = 0;

   rc = fskit_core_wlock( core );
   if( rc != 0 ) {
      return rc;
   }

   core->fskit_inode_alloc = inode_alloc;

   fskit_core_unlock( core );
   return 0;
}

// set the inode releaser
int fskit_core_inode_free_cb( struct fskit_core* core, fskit_inode_free_t inode_free ) {

   int rc = 0;

   rc = fskit_core_wlock( core );
   if( rc != 0 ) {
      return rc;
   }

   core->fskit_inode_free = inode_free;

   fskit_core_unlock( core );
   return 0;
}


// get the next free inode
// return 0 on error
uint64_t fskit_core_inode_alloc( struct fskit_core* core, struct fskit_entry* parent, struct fskit_entry* child ) {

   int rc = 0;

   rc = fskit_core_rlock( core );
   if( rc != 0 ) {
      return 0;
   }

   uint64_t next_inode = (*core->fskit_inode_alloc)( parent, child, core->app_fs_data );

   fskit_core_unlock( core );

   return next_inode;
}

// release an inode
int fskit_core_inode_free( struct fskit_core* core, uint64_t inode ) {

   int rc = 0;

   rc = fskit_core_rlock( core );
   if( rc != 0 ) {
      return rc;
   }

   rc = (*core->fskit_inode_free)( inode, core->app_fs_data );

   fskit_core_unlock( core );

   return rc;
}

// get the root node
// return pointer to the root on success
// return NULL if the root is deleted
struct fskit_entry* fskit_core_resolve_root( struct fskit_core* core, bool writelock ) {

   fskit_core_rlock( core );

   if( core->root.type == FSKIT_ENTRY_TYPE_DIR && !core->root.deletion_in_progress ) {
      // not dead
      if( writelock ) {
         fskit_entry_wlock( &core->root );
      }
      else {
         fskit_entry_rlock( &core->root );
      }

      fskit_core_unlock( core );
      return &core->root;
   }
   else {
      fskit_core_unlock( core );
      return NULL;
   }
}

// allocate an fskit entry 
struct fskit_entry* fskit_entry_new(void) {
   return CALLOC_LIST( struct fskit_entry, 1 );
}

// initialize an fskit entry
// this method does not fail.
int fskit_entry_init_lowlevel( struct fskit_entry* fent, uint8_t type, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode ) {

   int rc = 0;

   memset( fent, 0, sizeof(struct fskit_entry) );

   fent->type = type;
   fent->file_id = file_id;
   fent->owner = owner;
   fent->group = group;
   fent->mode = mode;
   fent->num_children = 0;

   return rc;
}


// common high-level initializer
int fskit_entry_init_common( struct fskit_entry* fent, uint8_t type, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode ) {

   int rc = 0;
   struct timespec now;

   rc = clock_gettime( CLOCK_REALTIME, &now );
   if( rc != 0 ) {
      fskit_error("clock_gettime rc = %d\n", rc );
      return rc;
   }

   fskit_entry_init_lowlevel( fent, type, file_id, owner, group, mode );

   fskit_entry_set_atime( fent, &now );
   fskit_entry_set_ctime( fent, &now );
   fskit_entry_set_mtime( fent, &now );

   pthread_rwlock_init( &fent->lock, NULL );

   fent->xattrs = NULL;

   return 0;
}

// high-level initializer: make a file
// return 0 on success
// return -ENOMEM on OOM
int fskit_entry_init_file( struct fskit_entry* fent, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode ) {

   int rc = 0;
   
   rc = fskit_entry_init_common( fent, FSKIT_ENTRY_TYPE_FILE, file_id, owner, group, mode );
   if( rc != 0 ) {
      fskit_error("fskit_entry_init_common(%" PRIX64 ") rc = %d\n", file_id, rc );
      return rc;
   }

   // for files, no further initialization needed
   return 0;
}


// high-level initializer: make a directory
// return 0 on success
// return -ENOMEM on OOM
int fskit_entry_init_dir( struct fskit_entry* fent, struct fskit_entry* parent, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode ) {

   int rc = 0;
   
   fskit_entry_set* children = fskit_entry_set_new( fent, parent );
   if( children == NULL ) {
      return -ENOMEM;
   }

   rc = fskit_entry_init_common( fent, FSKIT_ENTRY_TYPE_DIR, file_id, owner, group, mode );
   if( rc != 0 ) {
      fskit_error("fskit_entry_init_common(%" PRIX64 ") rc = %d\n", file_id, rc );
      fskit_safe_free( children );
      return rc;
   }

   fent->children = children;
   return 0;
}

// high-level initializer: make a fifo
// return 0 on success
// return -ENOMEM on OOM
int fskit_entry_init_fifo( struct fskit_entry* fent, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode ) {

   int rc = 0;
   
   rc = fskit_entry_init_common( fent, FSKIT_ENTRY_TYPE_FIFO, file_id, owner, group, mode );
   if( rc != 0 ) {
      fskit_error("fskit_entry_init_common(%" PRIX64 ") rc = %d\n", file_id, rc );
      return rc;
   }

   // nothing more to do for now
   return 0;
}

// high-level initializer: make a UNIX domain socket
// return 0 on success
// return -ENOMEM on OOM
int fskit_entry_init_sock( struct fskit_entry* fent, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode ) {

   int rc = 0;

   rc = fskit_entry_init_common( fent, FSKIT_ENTRY_TYPE_SOCK, file_id, owner, group, mode );
   if( rc != 0 ) {
      fskit_error("fskit_entry_init_common(%" PRIX64 ") rc = %d\n", file_id, rc );
      return rc;
   }

   // nothing more to do for now
   return 0;
}

// high-level initializer: make a character device
// return 0 on success
// return -ENOMEM on OOM
int fskit_entry_init_chr( struct fskit_entry* fent, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode, dev_t dev ) {

   int rc = 0;
   
   rc = fskit_entry_init_common( fent, FSKIT_ENTRY_TYPE_CHR, file_id, owner, group, mode );
   if( rc != 0 ) {
      fskit_error("fskit_entry_init_common(%" PRIX64 ") rc = %d\n", file_id, rc );
      return rc;
   }

   fent->dev = dev;
   return 0;
}

// high-level initializer: make a block device
// return 0 on success
// return -ENOMEM on OOM
int fskit_entry_init_blk( struct fskit_entry* fent, uint64_t file_id, uint64_t owner, uint64_t group, mode_t mode, dev_t dev ) {

   int rc = 0;

   rc = fskit_entry_init_common( fent, FSKIT_ENTRY_TYPE_BLK, file_id, owner, group, mode );
   if( rc != 0 ) {
      fskit_error("fskit_entry_init_common(%" PRIX64 ") rc = %d\n", file_id, rc );
      return rc;
   }

   fent->dev = dev;
   return 0;
}

// high-level initializer; make a symlink
// return 0 on success
// return -EINVAL if linkpath is NULL
// return -ENOMEM on OOM
int fskit_entry_init_symlink( struct fskit_entry* fent, uint64_t file_id, char const* linkpath ) {

   int rc = 0;
   
   if( linkpath == NULL ) {
      return -EINVAL;
   }
   
   char* symlink_target = NULL;
   
   if( linkpath != NULL ) {
       symlink_target = strdup_or_null( linkpath );
       if( symlink_target == NULL ) {
           return -ENOMEM;
       }
   }

   rc = fskit_entry_init_common( fent, FSKIT_ENTRY_TYPE_LNK, file_id, 0, 0, 0777 );
   if( rc != 0 ) {
      fskit_error("fskit_entry_init_common(%" PRIX64 ") rc = %d\n", file_id, rc );

      fskit_safe_free( symlink_target );
      return rc;
   }

   fent->symlink_target = symlink_target;
   
   if( symlink_target != NULL ) {
       fent->size = strlen( symlink_target );
   }
   else {
       fent->size = 0;
   }
   
   return 0;
}

// run user-supplied route callback to detach this fent
// mask -ENOSYS and -EPERM, i.e. the errors if no route exists or could be found
// fent must *not* be locked!
int fskit_run_user_detach( struct fskit_core* core, char const* path, struct fskit_entry* parent, struct fskit_entry* fent ) {

   int rc = 0;
   int cbrc = 0;
   struct fskit_route_dispatch_args dargs;
   char name[FSKIT_FILESYSTEM_NAMEMAX+1];
   
   memset( name, 0, FSKIT_FILESYSTEM_NAMEMAX+1 );
   fskit_basename( path, name );

   fskit_route_detach_args( &dargs, parent, name, fent->deletion_in_progress, fent->deletion_through_rename, fent->app_data );

   rc = fskit_route_call_detach( core, path, fent, &dargs, &cbrc );

   if( rc == -EPERM || rc == -ENOSYS ) {
      // no routes
      return 0;
   }

   else if( cbrc != 0 ) {
      // callback failed
      return cbrc;
   }

   else {
      // callback succeded
      return 0;
   }
}


// run user-supplied route callback to destroy this fent's inode data
// mask -ENOSYS and -EPERM, i.e. the errors if no route exists or could be found
// fent must *not* be locked!
int fskit_run_user_destroy( struct fskit_core* core, char const* path, struct fskit_entry* parent, struct fskit_entry* fent ) {

   int rc = 0;
   int cbrc = 0;
   struct fskit_route_dispatch_args dargs;
   char name[FSKIT_FILESYSTEM_NAMEMAX+1];
   
   memset( name, 0, FSKIT_FILESYSTEM_NAMEMAX+1 );
   fskit_basename( path, name );

   fskit_route_destroy_args( &dargs, parent, name, fent->deletion_through_rename, fent->app_data );

   rc = fskit_route_call_destroy( core, path, fent, &dargs, &cbrc );

   if( rc == -EPERM || rc == -ENOSYS ) {
      // no routes
      return 0;
   }

   else if( cbrc != 0 ) {
      // callback failed
      return cbrc;
   }

   else {
      // callback succeded
      return 0;
   }
}


// destroy a fskit entry
// NOTE: fent must be write-locked if is attached to the filesystem, or needlock must be true
// NOTE: the user-given callback will *NOT* be run.  Call fskit_entry_try_destroy for that.
int fskit_entry_destroy( struct fskit_core* core, struct fskit_entry* fent, bool needlock ) {

   // try to prevent double-frees
   if( fent->type == FSKIT_ENTRY_TYPE_DEAD ) {
      return 0;
   }

   if( needlock ) {
      fskit_entry_wlock( fent );
   }

   fskit_debug("fskit_entry_destroy %" PRIX64 "\n", fent->file_id);

   fent->type = FSKIT_ENTRY_TYPE_DEAD;      // next thread to hold this lock knows this is a dead entry

   // free common fields
   if( fent->children != NULL ) {
      fskit_entry_set_free( fent->children );
      fent->children = NULL;
   }

   if( fent->symlink_target != NULL ) {
      fskit_safe_free( fent->symlink_target );
      fent->symlink_target = NULL;
   }
   
   if( fent->xattrs != NULL ) {
      fskit_xattr_set_free( fent->xattrs );
      fent->xattrs = NULL;
   }
   
   (*core->fskit_inode_free)( fent->file_id, core->app_fs_data );
  
   if( needlock ) { 
       fskit_entry_unlock( fent );
   }
   pthread_rwlock_destroy( &fent->lock );

   return 0;
}


// try to destroy an fskit entry, if it is unlinked and no longer open
// return 0 if not destroyed
// return 1 if destroyed (even if the user-given detach callback fails)
// fent and parent must be write-locked.  If it is fully unlinked (i.e. this call will destroy it), it will be unlocked.
// NOTE: we mask any failures in the user callback.
// NOTE: even if this method returns an error code, the entry will be destroyed if it is no longer linked.
static int fskit_entry_try_destroy( struct fskit_core* core, char const* fs_path, struct fskit_entry* parent, struct fskit_entry* fent, int* cbrc ) {

   int rc = 0;
   uint64_t file_id = 0;

   if( fent->link_count <= 0 && fent->open_count <= 0 ) {

      if( fent->link_count < 0 ) {
         fskit_error("BUG: entry %p has a negative link count (%d)\n", fent, fent->link_count );
         exit(1);
      }
      
      if( fent->open_count < 0 ) {
         fskit_error("BUG: entry %p has a negative open count (%d)\n", fent, fent->open_count );
         exit(1);
      }
      
      // do the detach--nothing references it anymore
      // but, we should ref it ourselves, so this method won't succeed in another thread.
      fent->open_count++;
      file_id = fent->file_id;
      fskit_entry_unlock( fent );
     
      *cbrc = fskit_run_user_destroy( core, fs_path, parent, fent );
      if( *cbrc != 0 ) {
         fskit_error("WARN: fskit_run_user_destroy(%s) rc = %d\n", fs_path, *cbrc );
      }

      fskit_entry_destroy( core, fent, false );

      rc = 1;
   }

   return rc;
}


// try to destroy an fskit_entry, if it is unlinked and no longer open.
// Free it and decrement the number of children in the filesystem if we succeed.
// return 0 if not destroyed
// return 1 if destroyed (even if the user-given detach callback fails)
// fent and parent must be write-locked
int fskit_entry_try_destroy_and_free_ex( struct fskit_core* core, char const* fs_path, struct fskit_entry* parent, struct fskit_entry* fent, int* cbrc ) {

   int rc = 0;
   
   // see if we can destroy this....
   rc = fskit_entry_try_destroy( core, fs_path, parent, fent, cbrc );
   if( rc > 0 ) {
      
      // fent was unlocked and destroyed
      fskit_safe_free( fent );
   }

   return rc;
}


// default try-destroy-and-free: discard the callback return code
int fskit_entry_try_destroy_and_free( struct fskit_core* core, char const* fs_path, struct fskit_entry* parent, struct fskit_entry* fent ) {
   int cbrc = 0;
   return fskit_entry_try_destroy_and_free_ex( core, fs_path, parent, fent, &cbrc );
}


// try to garbage-collect a node.  That is, if it's deletion_in_progress field is true, try to destroy it.
// return 0 if detached but not destroyed
// return 1 if detached and destroyed (even if the user-given detach callback fails)
// set the garbage
// return -EEXIST if the entry can't be garbage collected
// NOTE: parent and child must be write-locked
int fskit_entry_try_garbage_collect( struct fskit_core* core, char const* path, struct fskit_entry* parent, struct fskit_entry* child ) {

   int rc = 0;
   uint64_t child_inode_id = 0;
   char path_basename[ FSKIT_FILESYSTEM_NAMEMAX + 1 ];

   // marked as garbage-collectable?
   if( child->deletion_in_progress ) {

      fskit_basename( path, path_basename );
      
      child_inode_id = child->file_id;

      // detach from the parent, but don't update mtime (since it was already detached)
      rc = fskit_entry_detach_lowlevel_ex( parent, path_basename, false );
      if( rc < 0 ) {
         
         if( rc == -ENOENT ) {
            // already removed 
            rc = 0;
         }
         else if( rc != -ENOTEMPTY ) {
            
            fskit_error("BUG: fskit_entry_detach_lowlevel_ex( %s ) rc = %d\n", path, rc );
            rc = -EIO;
         }
      }
      
      if( rc == 0 ) {
         
         // detached. try to destroy
         rc = fskit_entry_try_destroy_and_free( core, path, parent, child );
         if( rc >= 0 ) {

            if( rc > 0 ) {
               
               // destroyed! clear its name from the parent's children
               fskit_entry_set_remove( &parent->children, path_basename );
               parent->num_children--;
               
               fskit_debug( "Garbage-collected %s (%" PRIX64 ")\n", path, child_inode_id );
            }
         }
         else {

            // should not happen
            fskit_error( "BUG: fskit_entry_try_destroy_and_free( %s ) rc = %d\n", path, rc );

            rc = -EIO;
         }
      }
   }
   else {
      // not valid for garbage-collection
      rc = -EEXIST;
   }

   return rc;
}


// lock a file for reading
int fskit_entry_rlock2( struct fskit_entry* fent, char const* from_str, int line_no ) {
   if( FSKIT_GLOBAL_DEBUG_LOCKS ) {
      fskit_debug( "%p: %" PRIX64 ", from %s:%d\n", fent, fent->file_id, from_str, line_no );
   }

   int rc = pthread_rwlock_rdlock( &fent->lock );

   if( rc != 0 ) {
      fskit_error("pthread_rwlock_rdlock(%p) rc = %d (from %s:%d)\n", fent, rc, from_str, line_no );
   }
   else if( fent->type == FSKIT_ENTRY_TYPE_DEAD ) {
      pthread_rwlock_unlock( &fent->lock );
      return -ENOENT;
   }

   return rc;
}

// lock a file for writing
int fskit_entry_wlock2( struct fskit_entry* fent, char const* from_str, int line_no ) {
   if( FSKIT_GLOBAL_DEBUG_LOCKS ) {
      fskit_debug( "%p: %" PRIX64 ", from %s:%d\n", fent, fent->file_id, from_str, line_no );
   }

   int rc = pthread_rwlock_wrlock( &fent->lock );

   if( rc != 0 ) {
      fskit_error("pthread_rwlock_wrlock(%p) rc = %d (from %s:%d)\n", fent, rc, from_str, line_no );
   }
   else if( fent->type == FSKIT_ENTRY_TYPE_DEAD ) {
      pthread_rwlock_unlock( &fent->lock );
      return -ENOENT;
   }

   return rc;
}

// unlock a file
int fskit_entry_unlock2( struct fskit_entry* fent, char const* from_str, int line_no ) {
   int rc = pthread_rwlock_unlock( &fent->lock );
   if( rc == 0 ) {
      if( FSKIT_GLOBAL_DEBUG_LOCKS ) {
         fskit_debug( "%p: %" PRIX64 ", from %s:%d\n", fent, fent->file_id, from_str, line_no );
      }
   }
   else {
      fskit_error("pthread_rwlock_unlock(%p) rc = %d (from %s:%d)\n", fent, rc, from_str, line_no );
   }

   return rc;
}

// lock a file handle for reading
int fskit_file_handle_rlock( struct fskit_file_handle* fh ) {
   return pthread_rwlock_rdlock( &fh->lock );
}

// lock a file handle for writing
int fskit_file_handle_wlock( struct fskit_file_handle* fh ) {
   return pthread_rwlock_wrlock( &fh->lock );
}

// unlock a file handle
int fskit_file_handle_unlock( struct fskit_file_handle* fh ) {
   return pthread_rwlock_unlock( &fh->lock );
}

// lock a directory handle for reading
int fskit_dir_handle_rlock( struct fskit_dir_handle* dh ) {
   return pthread_rwlock_rdlock( &dh->lock );
}

// lock a directory handle for writing
int fskit_dir_handle_wlock( struct fskit_dir_handle* dh ) {
   return pthread_rwlock_wrlock( &dh->lock );
}

// unlock a directory handle
int fskit_dir_handle_unlock( struct fskit_dir_handle* dh ) {
   return pthread_rwlock_unlock( &dh->lock );
}

// read-lock a filesystem core
int fskit_core_rlock2( struct fskit_core* core, char const* from_str, int lineno ) {
   
   if( FSKIT_GLOBAL_DEBUG_LOCKS ) {
      fskit_debug( "%p: from %s:%d\n", core, from_str, lineno );
   }

   int rc = pthread_rwlock_rdlock( &core->lock );

   if( rc != 0 ) {
      fskit_error("pthread_rwlock_rdlock(%p) rc = %d (from %s:%d)\n", core, rc, from_str, lineno );
   }

   return rc;
}

// write-lock a filesystem core
int fskit_core_wlock2( struct fskit_core* core, char const* from_str, int lineno ) {

   if( FSKIT_GLOBAL_DEBUG_LOCKS ) {
      fskit_debug( "%p: from %s:%d\n", core, from_str, lineno );
   }

   int rc = pthread_rwlock_wrlock( &core->lock );
   
   if( rc != 0 ) {
      fskit_error("pthread_rwlock_wrlock(%p) rc = %d (from %s:%d)\n", core, rc, from_str, lineno );
   }

   return rc;
}

// unlock a filesystem core
int fskit_core_unlock2( struct fskit_core* core, char const* from_str, int lineno ) {

   if( FSKIT_GLOBAL_DEBUG_LOCKS ) {
      fskit_debug( "%p: from %s:%d\n", core, from_str, lineno );
   }

   int rc = pthread_rwlock_unlock( &core->lock );
   
   if( rc != 0 ) {
      fskit_error("pthread_rwlock_unlock(%p) rc = %d (from %s:%d)\n", core, rc, from_str, lineno );
   }

   return rc;
}

// read-lock routes
int fskit_core_route_rlock( struct fskit_core* core ) {
   return pthread_rwlock_rdlock( &core->route_lock );
}

// write-lock routes
int fskit_core_route_wlock( struct fskit_core* core ) {
   return pthread_rwlock_wrlock( &core->route_lock );
}

// unlock routes
int fskit_core_route_unlock( struct fskit_core* core ) {
   return pthread_rwlock_unlock( &core->route_lock );
}

// set user data in an fskit_entry (which must be write-locked)
// return 0 always
int fskit_entry_set_user_data( struct fskit_entry* ent, void* app_data ) {
   ent->app_data = app_data;
   return 0;
}

// set the file ID
// NOTE: don't do this outside of creat(), mkdir(), or mknod(), unless you want to suffer the consequences.
// ent must be write-locked
void fskit_entry_set_file_id( struct fskit_entry* ent, uint64_t file_id ) {
   ent->file_id = file_id;
}

// put a new set of children in place 
fskit_entry_set* fskit_entry_swap_children( struct fskit_entry* ent, fskit_entry_set* new_children ) {
   fskit_entry_set* old_children = ent->children;
   ent->children = new_children;
   return old_children;
}

// put a new set of xattrs in place
fskit_xattr_set* fskit_entry_swap_xattrs( struct fskit_entry* ent, fskit_xattr_set* new_xattrs ) {
   fskit_xattr_set* old_xattrs = ent->xattrs;
   ent->xattrs = new_xattrs;
   return old_xattrs;
}

// put a new symlink target, and replace the old one
// returns NULL if not a symlink 
char* fskit_entry_swap_symlink_target( struct fskit_entry* ent, char* new_symlink_target ) {
   if( ent->type != FSKIT_ENTRY_TYPE_LNK ) {
       return NULL;
   }
   
   char* old_target = ent->symlink_target;
   ent->symlink_target = new_symlink_target;
   
   if( new_symlink_target != NULL ) {
      ent->size = strlen(new_symlink_target);
   }
   else {
      ent->size = 0;
   }
   
   return old_target;
}

// Tag an inode for garbage-collection.  This cannot be undone.
// If this is a directory, remove its child set (replacing with an empty one) and set it to *children.
// The caller will need to deal with them--e.g. by attaching them somewhere else, unlinking them, etc.
// creat(), mknod(), and mkdir() will unlink this entry and overwrite it on collision.
// open(), stat(), etc. (anything that will resolve a path) will fail with -ENOENT
// readdir() will omit this entry.
// The inode will stay resident until its link count reaches zero; then it will be destroyed.
// The application can hasten this by unlinking the inode in e.g. a deferred, asynchronous thread.
// ent must be write-locked 
// return 0 on success 
// return -ENOMEM on OOM 
// return -EIO if ent is a directory and lacks a .. entry.
int fskit_entry_tag_garbage( struct fskit_entry* ent, fskit_entry_set** children ) {
    
    fskit_debug("Tag %" PRIX64 " as garbage (link count %d, open count %d)\n", ent->file_id, ent->link_count, ent->open_count );
    
    if( ent->type == FSKIT_ENTRY_TYPE_DIR ) {
        
        // new, empty child set 
        struct fskit_entry* parent = fskit_entry_set_find_name( ent->children, ".." );
        if( parent == NULL ) {
            
            // should *never* happen 
            fskit_error("BUG: directory %" PRIX64 " does not have a parent entry\n", ent->file_id );
            return -EIO;
        }
        
        fskit_entry_set* empty_children = fskit_entry_set_new( ent, parent );
        if( empty_children == NULL ) {
            
            // OOM 
            return -ENOMEM;
        }
        
        // do the swap 
        *children = ent->children;
        ent->children = empty_children;
        ent->num_children = 0;
        ent->deletion_in_progress = true;
    }
    else {
       ent->deletion_in_progress = true;
    }

    return 0;
}

// get the file's file ID (i.e. its inode number)
uint64_t fskit_entry_get_file_id( struct fskit_entry* ent ) {
   return ent->file_id;
}

// get user-given data for this entry
void* fskit_entry_get_user_data( struct fskit_entry* ent ) {
   return ent->app_data;
}

// get a reference (not a copy) of the path used to open this handle
char* fskit_file_handle_get_path( struct fskit_file_handle* fh ) {
   return fh->path;
}

// get the inode structure for this file handle
struct fskit_entry* fskit_file_handle_get_entry( struct fskit_file_handle* fh ) {
   return fh->fent;
}

// get the file handle's user-given handle data
void* fskit_file_handle_get_user_data( struct fskit_file_handle* fh ) {
   return fh->app_data;
}

// get a reference (not a copy) of the path used to open this handle
char* fskit_dir_handle_get_path( struct fskit_dir_handle* dh ) {
   return dh->path;
}

// get the inode structure for this file handle
struct fskit_entry* fskit_dir_handle_get_entry( struct fskit_dir_handle* dh ) {
   return dh->dent;
}

// get the file handle's user-given handle data
void* fskit_dir_handle_get_user_data( struct fskit_dir_handle* dh ) {
   return dh->app_data;
}

// get the user-given data for the filesystem
void* fskit_core_get_user_data( struct fskit_core* core ) {
   return core->app_fs_data;
}

// get type (ent must be read-locked)
uint8_t fskit_entry_get_type( struct fskit_entry* ent ) {
   return ent->type;
}

// get deletion-in-progress flag (must be read-locked)
bool fskit_entry_get_deletion_in_progress( struct fskit_entry* ent ) {
   return ent->deletion_in_progress;
}

// get a pointer to the children 
fskit_entry_set* fskit_entry_get_children( struct fskit_entry* ent ) {
   return ent->children;
}

// get a pointer to the xattrs 
fskit_xattr_set* fskit_entry_get_xattrs( struct fskit_entry* ent ) {
   return ent->xattrs;
}

// get owner (ent must be read-locked)
uint64_t fskit_entry_get_owner( struct fskit_entry* ent ) {
   return ent->owner;
}

// get group (ent must be read-locked)
uint64_t fskit_entry_get_group( struct fskit_entry* ent ) {
   return ent->group;
}
 
// get atime (ent must be read-locked)
void fskit_entry_get_atime( struct fskit_entry* ent, int64_t* atime_sec, int32_t* atime_nsec ) {
   *atime_sec = ent->atime_sec;
   *atime_nsec = ent->atime_nsec;
}
 
// get mtime (ent must be read-locked)
void fskit_entry_get_mtime( struct fskit_entry* ent, int64_t* mtime_sec, int32_t* mtime_nsec ) {
   *mtime_sec = ent->mtime_sec;
   *mtime_nsec = ent->mtime_nsec;
}

// get ctime (ent must be read-locked)
void fskit_entry_get_ctime( struct fskit_entry* ent, int64_t* ctime_sec, int32_t* ctime_nsec ) {
   *ctime_sec = ent->ctime_sec;
   *ctime_nsec = ent->ctime_nsec;
}

// get size (ent must be read-locked)
off_t fskit_entry_get_size( struct fskit_entry* ent ) {
   return ent->size;
}

// get device major/minor, if this is a special file (ent must be read-lodked)
dev_t fskit_entry_get_rdev( struct fskit_entry* ent ) {
   return ent->dev;
}

// get permission bits 
mode_t fskit_entry_get_mode( struct fskit_entry* ent ) {
   return ent->mode;
}

// get link count 
int32_t fskit_entry_get_link_count( struct fskit_entry* ent ) {
   return ent->link_count;
}

// get number of children.  if this is not a directory, return -1
// NOTE: ent must be read-locked
int64_t fskit_entry_get_num_children( struct fskit_entry* ent ) {
   
   if( ent->children != NULL ) {
      
      return ent->num_children;
   }
   else {
      
      return -1;
   }
}

// free up all xattrs in an fskit_xattr_set
int fskit_xattr_set_free( fskit_xattr_set* xattrs ) {
   
   if( xattrs == NULL ) {
       return 0;
   }
   
   fskit_xattr_set_itr itr;   
   fskit_xattr_set* dp = NULL;
   fskit_xattr_set* old_dp = NULL;

   for( dp = fskit_xattr_set_begin( &itr, xattrs ); dp != NULL; ) {
      
      fskit_safe_free( dp->name );
      fskit_safe_free( dp->value );
      
      old_dp = dp;
      dp = fskit_xattr_set_next( &itr );
      
      fskit_safe_free( old_dp );
   }
   
   return 0;
}

// create a new xattr set 
fskit_xattr_set* fskit_xattr_set_new(void) {
   
   return CALLOC_LIST( fskit_xattr_set, 1 );
}

// insert an xattr.  duplicates name and value.
// return 0 on success
// return -EINVAL on NULL data
// return -EEXIST if the member is already present, and XATTR_CREATE is set in flags 
// return -ENOATTR if the member is not present, and XATTR_REPLACE is set in flags
// return -ENOMEM on OOM 
int fskit_xattr_set_insert( fskit_xattr_set** set, char const* name, char const* value, size_t value_len, int flags ) {
   
   char* name_dup = NULL;
   char* value_dup = NULL;
   fskit_xattr_set* member = NULL;
   
   fskit_xattr_set lookup;

   // sanity check 
   if( name == NULL || value == NULL ) {
      return -EINVAL;
   }
   
   if( *set != NULL ) {
       
      memset( &lookup, 0, sizeof(fskit_xattr_set) );
      lookup.name = (char*)name;
    
      member = sglib_fskit_xattr_set_find_member( *set, &lookup );
    
      if( member != NULL && (flags & XATTR_CREATE) ) {
        
          // can't exist yet
          return -EEXIST;
      }
    
      if( member == NULL && (flags & XATTR_REPLACE) ) {
        
          // needs to exist first 
          return -ENOATTR;
      }
      
      // remove 
      fskit_xattr_set_remove( set, name );
      member = NULL;
   }
   
   // put in place
   name_dup = strdup_or_null( name );
   if( name_dup == NULL ) {
      return -ENOMEM;
   }
   
   value_dup = CALLOC_LIST( char, value_len );
   if( value_dup == NULL ) {
      
      fskit_safe_free( name_dup );
      return -ENOMEM;
   }

   member = CALLOC_LIST( fskit_xattr_set, 1 );
   if( member == NULL ) {
        
       fskit_safe_free( name_dup );
       fskit_safe_free( value_dup );
       return -ENOMEM;
   }
    
   member->name = name_dup;
   memcpy( value_dup, value, value_len );
   
   member->value = value_dup;
   member->value_len = value_len;
   
   sglib_fskit_xattr_set_add( set, member );
   return 0;
}


// look up an xattr by name 
// return the pointer to the value on success, and set *len to be the length 
// return NULL if not found.
char const* fskit_xattr_set_find( fskit_xattr_set* set, char const* name, size_t* len ) {

   fskit_xattr_set* member = NULL;
   fskit_xattr_set lookup;
   
   memset( &lookup, 0, sizeof(fskit_xattr_set));
   lookup.name = (char*)name;
   
   member = sglib_fskit_xattr_set_find_member( set, &lookup );
   if( member == NULL ) {
      
      return NULL;
   }
   
   *len = member->value_len;
   return member->value;
}


// remove an xattr by name 
// return true if removed 
// return false if not present 
bool fskit_xattr_set_remove( fskit_xattr_set** set, char const* name ) {
   
   int rc = 0;
   fskit_xattr_set lookup;
   fskit_xattr_set* member = NULL;
   
   memset( &lookup, 0, sizeof(fskit_xattr_set));
   lookup.name = (char*)name;
   
   sglib_fskit_xattr_set_delete_if_member( set, &lookup, &member );
   if( member != NULL ) {
      
      // free up 
      fskit_safe_free( member->name );
      fskit_safe_free( member->value );
      fskit_safe_free( member );
      return true;
   }
   else {
      
      return false;
   }
}


// get the number of xattrs 
unsigned int fskit_xattr_set_count( fskit_xattr_set* set ) {
   
   return sglib_fskit_xattr_set_len( set );
}


// get the name 
char const* fskit_xattr_set_name( fskit_xattr_set* set ) {
   
   return set->name;
}


// get the value
char const* fskit_xattr_set_value( fskit_xattr_set* set ) {
   
   return set->value;
}


// get the value length 
size_t fskit_xattr_set_value_len( fskit_xattr_set* set ) {
   
   return set->value_len;
}

// start iterating over a set of xattrs
fskit_xattr_set* fskit_xattr_set_begin( fskit_xattr_set_itr* itr, fskit_xattr_set* xattrs ) {
   
   return sglib_fskit_xattr_set_it_init_inorder( itr, xattrs );
}

// get the next xattr in a set of xattrs
fskit_xattr_set* fskit_xattr_set_next( fskit_xattr_set_itr* itr ) {
   
   return sglib_fskit_xattr_set_it_next( itr );
}

// setup metadata
struct fskit_inode_metadata* fskit_inode_metadata_new() {
   return CALLOC_LIST( struct fskit_inode_metadata, 1 ); 
}

int fskit_inode_metadata_free( struct fskit_inode_metadata* imd ) {
   fskit_safe_free( imd );
   return 0;
}

uint64_t fskit_inode_metadata_get_inventory( struct fskit_inode_metadata* imd ) {
   return imd->fields;
}

mode_t fskit_inode_metadata_get_mode( struct fskit_inode_metadata* imd ) {
   return imd->mode;
}

uint64_t fskit_inode_metadata_get_owner( struct fskit_inode_metadata* imd ) {
   return imd->owner;
}

uint64_t fskit_inode_metadata_get_group( struct fskit_inode_metadata* imd ) {
   return imd->group;
}

void fskit_inode_metadata_set_mode( struct fskit_inode_metadata* imd, mode_t mode ) {
   imd->mode = mode;
   imd->fields |= FSKIT_INODE_METADATA_MODE;
}

void fskit_inode_metadata_set_owner( struct fskit_inode_metadata* imd, uint64_t owner ) {
   imd->owner = owner;
   imd->fields |= FSKIT_INODE_METADATA_OWNER;
}

void fskit_inode_metadata_set_group( struct fskit_inode_metadata* imd, uint64_t group ) {
   imd->group = group;
   imd->fields |= FSKIT_INODE_METADATA_GROUP;
}

