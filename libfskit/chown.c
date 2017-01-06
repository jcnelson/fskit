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

#include <fskit/chown.h>
#include <fskit/path.h>
#include <fskit/entry.h>

#include "fskit_private/private.h"

// directly set the owner and group of an inode 
// always succeeds
// NOTE: fent must be write-locked 
int fskit_entry_set_owner_and_group( struct fskit_entry* fent, uint64_t new_user, uint64_t new_group ) {
   
   fent->owner = new_user;
   fent->group = new_group;
   return 0;
}

// directly set the owner of an inode 
// always succeeds
// NOTE: fent must be write-locked
int fskit_entry_set_owner( struct fskit_entry* fent, uint64_t new_user ) {
   
   fent->owner = new_user;
   return 0;
}

// directly set the group of an inode 
// always succeeds 
// NOTE: fent must be write-locked 
int fskit_entry_set_group( struct fskit_entry* fent, uint64_t new_group ) {

   fent->group = new_group;
   return 0;
}

// run the user-installed setmetadata handler
// return 0 on success, or if there are no routes
// return negative on callback failure
// fent *must* be write-locked
int fskit_run_user_setmetadata( struct fskit_core* core, char const* path, struct fskit_entry* fent, struct fskit_inode_metadata* imd ) {

   // route?
   struct fskit_route_dispatch_args dargs;
   int rc = 0;
   int cbrc = 0;

   fskit_route_setmetadata_args( &dargs, imd );
   rc = fskit_route_call_setmetadata( core, path, fent, &dargs, &cbrc );

   if( rc == -EPERM || rc == -ENOSYS ) {

      // no routes
      return 0;
   }
   else if( cbrc != 0 ) {

      // callback failed
      return cbrc;
   }
   else {

      // success!
      return 0;
   }
}

// change the owner of a path.
// the file must be owned by the given user.
// NOTE: no ingroup-checking occurs--if the caller is the owner, the new_group can be arbitrary.
// the caller should verify that the new_group is allowed by its security model.
// Return 0 on success, negative on error:
// -ENOMEM if oom
// -EPERM if we don't own the file
// -EACCES if the caller can't search a component of the path
// -ENOTDIR if the path has a parent non-directory
// -ENOENT if the entry doesn't exist
int fskit_chown( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, uint64_t new_user, uint64_t new_group ) {

   int err = 0;
   struct fskit_inode_metadata* imd = fskit_inode_metadata_new();
   if( imd == NULL ) {
      return -ENOMEM;
   }

   struct fskit_entry* fent = fskit_entry_resolve_path( core, path, user, group, true, &err );
   if( !fent || err ) {
      if( !err ) {
         err = -ENOMEM;
      }

      fskit_inode_metadata_free(imd);
      return err;
   }

   // can't chown unless we own the file
   if( fent->owner != user ) {
      fskit_entry_unlock( fent );
      fskit_inode_metadata_free(imd);
      return -EPERM;
   }
  
   fskit_inode_metadata_set_owner( imd, new_user );
   fskit_inode_metadata_set_group( imd, new_group );

   err = fskit_run_user_setmetadata( core, path, fent, imd );
   fskit_inode_metadata_free( imd );

   if( err != 0 ) {
      // failed 
      fskit_entry_unlock( fent );
      fskit_error("User callback to set owner and group failed, rc = %d\n", err);
      return err;
   }

   // success! propagate
   fskit_entry_set_owner_and_group( fent, new_user, new_group );
   fskit_entry_unlock( fent );
   return err;
}
