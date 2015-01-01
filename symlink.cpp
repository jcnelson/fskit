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

#include "symlink.h"
#include "path.h"
#include "fskit.h"
#include "util.h"

// symlink to 'target' from 'linkpath'.  The caller must have write permission in 'linkpath's directory.
// return 0 on success
// return -ENOTDIR if target is not a directory
// return -EACCES if target is not writable to the user/group
// return -ENOMEM if we run out of memory
// return -EIO if we fail to allocate and set up the symlink inode
// return negative errno if path resolution for the parent of target fails.
int fskit_symlink( struct fskit_core* core, char const* target, char const* linkpath, uint64_t user, uint64_t group ) {

   int err = 0;
   int rc = 0;

   char parent_path[ PATH_MAX+1 ];
   char child_name[ FSKIT_FILESYSTEM_NAMEMAX+1 ];

   struct fskit_entry* parent;
   struct fskit_entry* child;
   uint64_t file_id = 0;

   memset( parent_path, 0, PATH_MAX+1 );
   memset( child_name, 0, FSKIT_FILESYSTEM_NAMEMAX+1 );

   fskit_dirname( linkpath, parent_path );
   fskit_basename( linkpath, child_name );

   // get the parent
   parent = fskit_entry_resolve_path( core, parent_path, user, group, true, &err );
   if( parent == NULL || err != 0 ) {
      return err;
   }

   // caller must have write permission
   if( !FSKIT_ENTRY_IS_WRITEABLE( parent->mode, parent->owner, parent->group, user, group ) ) {
      fskit_entry_unlock( parent );
      return -EACCES;
   }

   // must be a directory
   if( parent->type != FSKIT_ENTRY_TYPE_DIR ) {
      fskit_entry_unlock( parent );
      return -ENOTDIR;
   }

   // verify the child doesn't exist
   child = fskit_entry_set_find_name( parent->children, child_name );

   if( child != NULL ) {
      // exists
      fskit_entry_unlock( parent );
      return -EEXIST;
   }

   // allocate
   child = CALLOC_LIST( struct fskit_entry, 1 );
   if( child == NULL ) {

      fskit_entry_unlock( parent );
      return -ENOMEM;
   }

   file_id = fskit_core_inode_alloc( core, parent, child );
   if( file_id == 0 ) {

      fskit_entry_unlock( parent );
      safe_free( child );
      return -EIO;
   }

   // populate
   rc = fskit_entry_init_symlink( child, file_id, child_name, target );
   if( rc != 0 ) {

      fskit_entry_destroy( core, child, true );
      safe_free( child );

      fskit_entry_unlock( parent );
      return -EIO;
   }

   // insert
   rc = fskit_entry_set_insert( parent->children, child_name, child );
   if( rc != 0 ) {

      fskit_entry_destroy( core, child, true );
      safe_free( child );

      fskit_entry_unlock( parent );
      return -EIO;
   }

   // done!
   fskit_entry_unlock( parent );
   return 0;
}
