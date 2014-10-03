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

#include "chmod.h"
#include "path.h"

// change the mode of the file.  All bits (including suid, sgid, and sticky) are supported
// only the owner can change the file's mode.
// Return 0 on success, negative on error:
// -ENOMEM if oom
// -EPERM if the caller doesn't own the file 
// -EACCES if the caller can't search a component of the path 
// -ENOTDIR if the path has a parent non-directory 
// -ENOENT if the entry doesn't exist
int fskit_chmod( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, mode_t mode ) {
   
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
   
   fent->mode = mode;
   
   fskit_entry_unlock( fent );

   return err;
}