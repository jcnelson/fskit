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

#include "getxattr.h"
#include "path.h"

// wrapper around fgetxattr, which resolves the path to the fskit entry 
// returns whatever fgetxattr returns, plus any errors in path resolution
int fskit_getxattr( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, char const* name, char* value, size_t size ) {
   
   int err = 0;
   int rc = 0;
   
   // get the fent
   struct fskit_entry* fent = fskit_entry_resolve_path( core, path, user, group, false, &err );
   if( fent == NULL || err != 0 ) {
      return err;
   }

   // get the xattr
   rc = fskit_fgetxattr( core, fent, name, value, size );
   
   fskit_entry_unlock( fent );
   
   return rc;
}

// get an xattr value.  fent must be at least read-locked
// return the length of the attribute on success 
// on error:
// * -ENOATTR if the attribute doesn't exist 
// * -ERANGE if the buffer isn't big enough 
// * -ENOMEM if there isn't enough memory
// * anything due to a failure in stat
// if size == 0 or value == NULL, then just return the length of the attribute requested
int fskit_fgetxattr( struct fskit_core* core, struct fskit_entry* fent, char const* name, char* value, size_t size ) {
   
   fskit_xattr_rlock( fent );
   
   string name_s;
   fskit_xattr_set::iterator itr;
   
   try {
      
      name_s = string( name );
      
      itr = fent->xattrs->find( name_s );
      
   } catch( bad_alloc& ba ) {
      
      fskit_xattr_unlock( fent );
      return -ENOMEM;
   }
   
   if( itr == fent->xattrs->end() ) {
      // not found 
      fskit_xattr_unlock( fent );
      return -ENOATTR;
   }
   
   string& value_s = itr->second;
   int value_len = value_s.size();
   
   // size query?
   if( value == NULL || size == 0 ) {
      
      fskit_xattr_unlock( fent );
      return value_len;
   }
   
   // enough space?
   if( value_s.size() > size ) {
      
      fskit_xattr_unlock( fent );
      return -ERANGE;
   }
   
   int ret = value_s.size();
   
   // success!
   memcpy( value, value_s.data(), value_s.size() );
   
   fskit_xattr_unlock( fent );
   
   return ret;
}
