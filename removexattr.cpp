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

#include "removexattr.h"
#include "path.h"

// remove an xattr.
// returns whatever fremovexattr returns, plus whatever error codes path resolution can return
int fskit_removexattr( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, char const* name ) {
   
   int err = 0;
   int rc = 0;
   
   // get the fent
   struct fskit_entry* fent = fskit_entry_resolve_path( core, path, user, group, false, &err );
   if( fent == NULL || err != 0 ) {
      return err;
   }

   // get the xattr
   rc = fskit_fremovexattr( core, fent, name );
   
   fskit_entry_unlock( fent );
   
   return rc;
}

// remove an xattr.
// return 0 on success
// return -ENOATTR if the attribute doesn't exist 
// return -ENOMEM if we run out of memory 
int fskit_fremovexattr( struct fskit_core* core, struct fskit_entry* fent, char const* name ) {
   
   fskit_xattr_wlock( fent );
   
   try {
      
      fskit_xattr_set::iterator itr = fent->xattrs->find( string(name) );
      
      if( itr == fent->xattrs->end() ) {
         
         fskit_xattr_unlock( fent );
         return -ENOATTR;
      }
      
      // remove it 
      fent->xattrs->erase( itr );
   }
   catch( bad_alloc& ba ) {
      fskit_xattr_unlock( fent );
      return -ENOMEM;
   }
   
   fskit_xattr_unlock( fent );
   return 0;
}