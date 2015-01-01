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

#include "deferred.h"
#include "path.h"
#include "wq.h"
#include "util.h"

// fskit remove_all context
struct fskit_deferred_remove_all_ctx {

   struct fskit_core* core;
   char* fs_path;               // path to the (root) entry
   struct fskit_entry* child;   // only used for removing a single entry
   fskit_entry_set* children;   // only used for removing a tree of entries
};


// helper to asynchronously reap a directory of inodes, recursively.
// used by fskit_entry_remove_all; invoked by the core deferred work queue
static int fskit_deferred_remove_all_cb( struct fskit_wreq* wreq, void* cls ) {

   struct fskit_deferred_remove_all_ctx* ctx = (struct fskit_deferred_remove_all_ctx*)cls;
   int rc = 0;

   fskit_debug("DEFERRED: garbage-collect all children of %s\n", ctx->fs_path);

   rc = fskit_detach_all( ctx->core, ctx->fs_path, ctx->children );

   if( rc != 0 ) {
      fskit_error("LEAK: fskit_detach_all(%s) rc = %d\n", ctx->fs_path, rc );
   }

   safe_free( ctx->fs_path );
   safe_delete( ctx->children );
   safe_free( ctx );

   return 0;
}


// helper to asynchronously try to reap a node, ensuring that if it's not open, it gets destroyed.
// used by fskit_entry_remove; invoked by the core deferred work queue
static int fskit_deferred_remove_cb( struct fskit_wreq* wreq, void* cls ) {

   struct fskit_deferred_remove_all_ctx* ctx = (struct fskit_deferred_remove_all_ctx*)cls;
   int rc = 0;

   fskit_debug("DEFERRED: garbage-collect %s\n", ctx->fs_path);

   // NOTE: safe, since in fskit_entry_remove we incremented the open count.
   // the child will not have been freed yet.
   fskit_entry_wlock( ctx->child );

   ctx->child->open_count--;

   rc = fskit_entry_try_destroy_and_free( ctx->core, ctx->fs_path, ctx->child );
   if( rc >= 0 ) {

      // success!
      if( rc == 0 ) {
         // not destroyed
         fskit_entry_unlock( ctx->child );
      }
      else {
         // destroyed
         rc = 0;
      }
   }
   else {
      fskit_error("LEAK: fskit_entry_try_destroy_and_free(%s) rc = %d\n", ctx->fs_path, rc );
   }

   return 0;
}


// Mark an inode as removed from the filesystem.
// This will cause all subsequent path resolutions on this inode to fail, regardless of its parents.
// The inode will be garbage-collected asynchronously
// child must be write-locked.
// return 0 on success
// return -EISDIR if the child is a directory (use fskit_entry_remove_all for directories)
// NOTE: child must be write-locked
int fskit_deferred_remove( struct fskit_core* core, char const* child_path, struct fskit_entry* child ) {

   struct fskit_deferred_remove_all_ctx* ctx = NULL;
   struct fskit_wreq work;
   int rc = 0;

   if( child->type == FSKIT_ENTRY_TYPE_DIR ) {
      return -EISDIR;
   }

   ctx = CALLOC_LIST( struct fskit_deferred_remove_all_ctx, 1 );
   if( ctx == NULL ) {
      return -ENOMEM;
   }

   // queue for destruction
   ctx->core = core;
   ctx->fs_path = strdup( child_path );
   ctx->child = child;

   if( ctx->fs_path == NULL ) {
      safe_free( ctx );
      return -ENOMEM;
   }

   // mark the child as dead--it won't be resolvable again
   child->link_count = 0;
   child->deletion_in_progress = true;

   // reference the child--don't want it to disappear on us
   child->open_count++;

   // defer deletion
   fskit_wreq_init( &work, fskit_deferred_remove_cb, ctx, 0 );

   rc = fskit_wq_add( core->deferred, &work );
   if( rc != 0 ) {
      // not running, or OOM
      fskit_error("fskit_wq_add( fskit_deferred_remove_cb, %s ) rc = %d\n", child_path, rc );
      return -EAGAIN;
   }

   return 0;
}


// Mark a directory and all of its children as removed from the filesystem.
// This will cause all subsequent path resolutions on this inode and its children to fail.
// They will be garbage-collected asynchronously.
// return 0 on success
// return -ENOTDIR if child is not a directory
// NOTE: child must be write-locked
int fskit_deferred_remove_all( struct fskit_core* core, char const* child_path, struct fskit_entry* child ) {

   struct fskit_deferred_remove_all_ctx* ctx = NULL;
   struct fskit_entry* parent = NULL;
   struct fskit_wreq work;
   int rc = 0;

   if( child->type != FSKIT_ENTRY_TYPE_DIR ) {
      return -ENOTDIR;
   }

   parent = fskit_entry_set_find_name( child->children, ".." );
   if( parent == NULL ) {
      // malformatted child
      return -EINVAL;
   }

   ctx = CALLOC_LIST( struct fskit_deferred_remove_all_ctx, 1 );
   if( ctx == NULL ) {
      return -ENOMEM;
   }

   // queue all children for destruction
   ctx->core = core;
   ctx->fs_path = strdup( child_path );
   ctx->children = child->children;

   if( ctx->fs_path == NULL ) {
      safe_free( ctx );
      return -ENOMEM;
   }

   // make sure we can't resolve the children again
   child->children = fskit_entry_set_new( child, parent );

   if( child->children == NULL ) {
      safe_free( ctx->fs_path );
      safe_free( ctx );
      return -ENOMEM;
   }

   // mark the child as dead--it won't be resolvable again
   child->link_count = 0;
   child->deletion_in_progress = true;

   // defer deletion
   fskit_wreq_init( &work, fskit_deferred_remove_all_cb, ctx, 0 );

   rc = fskit_wq_add( core->deferred, &work );
   if( rc != 0 ) {
      // not running, or OOM
      fskit_error("fskit_wq_add( fskit_deferred_remove_all_cb, %s ) rc = %d\n", child_path, rc );
      return -EAGAIN;
   }

   return 0;
}
