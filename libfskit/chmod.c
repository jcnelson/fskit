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

#include <fskit/chmod.h>
#include <fskit/path.h>
#include <fskit/entry.h>

#include "fskit_private/private.h"

// directly set the mode of an inode 
// always succeeds
// NOTE: fent must be write-locked 
int fskit_entry_set_mode( struct fskit_entry* fent, mode_t mode ) {
   
   fent->mode = mode;
   return 0;
}

// change the mode of the file.  All bits (including suid, sgid, and sticky) are supported
// only the owner can change the file's mode.
// Return 0 on success, negative on error:
// -ENOMEM if oom
// -EPERM if the calling user doesn't have permission to make the change
// -EACCES if the caller can't search a component of the path, or doesn't own the file
// -ENOTDIR if the path has a parent non-directory
// -ENOENT if the entry doesn't exist
int fskit_chmod( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, mode_t mode ) {

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

   fskit_inode_metadata_set_mode( imd, mode );

   // pass to user-given route 
   err = fskit_run_user_setmetadata(core, path, fent, imd );
   if( err != 0 ) {
      fskit_error("Failed to run user setmetadata; rc = %d\n", err);
      fskit_entry_unlock( fent );
      return err;
   }

   fskit_entry_set_mode( fent, mode );
   fskit_entry_unlock( fent );

   return err;
}
