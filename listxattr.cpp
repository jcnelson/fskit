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

#include "listxattr.h"
#include "path.h"

int fskit_listxattr( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, char* list, size_t size ) {
   
   int err = 0;
   int rc = 0;
   
   // get the fent
   struct fskit_entry* fent = fskit_entry_resolve_path( core, path, user, group, false, &err );
   if( fent == NULL || err != 0 ) {
      return err;
   }

   // get the xattr
   rc = fskit_flistxattr( core, fent, list, size );
   
   fskit_entry_unlock( fent );
   
   return rc;
}


// what's the total length of all attributes names (null-terminated)?
static int fskit_listxattr_len( fskit_xattr_set* xattrs ) {
   
   int size = 0;
   
   for( fskit_xattr_set::iterator itr = xattrs->begin(); itr != xattrs->end(); itr++ ) {
      
      size += itr->first.size() + 1;
   }
   
   return size;
}

// copy the xattr names into a buffer.
// NOTE: no input validation occurs here.  The caller must ensure that list has enough space to hold all names, separated by \0's
static void fskit_listxattr_copy_names( fskit_xattr_set* xattrs, char* list, size_t size ) {
   
   off_t offset = 0;
   
   for( fskit_xattr_set::iterator itr = xattrs->begin(); itr != xattrs->end(); itr++ ) {
      
      memcpy( list + offset, itr->first.data(), itr->first.size() );
      
      offset += itr->first.size();
      
      *(list + offset) = '\0';
      
      offset++;
   }
}


// get the list of all xattr names 
// return the length of the name list on success
// return on error:
// * -ERANGE if the buffer is too short 
// if list == NULL or size == 0, then just return the length of the name list
int fskit_flistxattr( struct fskit_core* core, struct fskit_entry* fent, char* list, size_t size ) {
   
   int total_size = 0;
   
   fskit_xattr_rlock( fent );
   
   // what's the total size?
   total_size = fskit_listxattr_len( fent->xattrs );
   
   // just a length query?
   if( list == NULL || size == 0 ) {
      fskit_xattr_unlock( fent );
      return total_size;
   }
   
   // range check 
   if( (unsigned)total_size > size ) {
      fskit_xattr_unlock( fent );
      return -ERANGE;
   }
   
   // copy everything in 
   fskit_listxattr_copy_names( fent->xattrs, list, size );
   
   fskit_xattr_unlock( fent );
   
   return total_size;
}