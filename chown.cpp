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


#include "chown.h"
#include "path.h"

// change the owner of a path.
// the file must be owned by the given user.
// NOTE: no ingroup-checking occurs--if the caller is the owner, the new_group can be arbitrary.
// the caller should verify that the new_group is allowed by its security model.
// Return 0 on success, negative on error:
// -ENOMEM if oom
// -EPERM if the caller doesn't own the file 
// -EACCES if the caller can't search a component of the path 
// -ENOTDIR if the path has a parent non-directory 
// -ENOENT if the entry doesn't exist
int fskit_chown( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, uint64_t new_user, uint64_t new_group ) {
   
   int err = 0;
   
   struct fskit_entry* fent = fskit_entry_resolve_path( core, path, user, group, true, &err );
   if( !fent || err ) {
      if( !err ) {
         err = -ENOMEM;
      }
      return err;
   }

   // can't chown unless we own the file
   if( fent->owner != user ) {
      fskit_entry_unlock( fent );
      return -EPERM;
   }
   
   fent->owner = new_user;
   fent->group = new_group;
   
   fskit_entry_unlock( fent );

   return err;
}