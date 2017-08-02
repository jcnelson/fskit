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

#include <fskit/listxattr.h>
#include <fskit/path.h>

#include "fskit_private/private.h"

// get the user-supplied xattr data for listxattr
// return > 0 if the callback handled the request
// return 0 if the callback did not handle the request, but fskit should
// return negative on error
int fskit_run_user_listxattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, char* xattr_buf, size_t xattr_buf_len ) {

   int rc = 0;
   int cbrc = 0;
   struct fskit_route_dispatch_args dargs;
   
   fskit_route_listxattr_args( &dargs, xattr_buf, xattr_buf_len );
   rc = fskit_route_call_listxattr( core, path, fent, &dargs, &cbrc );

   if( rc == -EPERM ) {
      // no routes 
      return 0;
   }
   else if( rc < 0 ) {
      return rc;
   }

   return cbrc;
}


int fskit_listxattr( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, char* list, size_t size ) {

   int err = 0;
   int rc = 0;

   // get the fent
   struct fskit_entry* fent = fskit_entry_resolve_path( core, path, user, group, false, &err );
   if( fent == NULL || err != 0 ) {
      return err;
   }

   // get the xattr
   rc = fskit_flistxattr( core, path, fent, list, size );

   fskit_entry_unlock( fent );

   return rc;
}


// what's the total length of all attributes names (null-terminated)?
static int fskit_listxattr_len( fskit_xattr_set* xattrs ) {

   int size = 0;
   char const* name = NULL;
   fskit_xattr_set_itr itr;
   fskit_xattr_set* xattr = NULL;

   if( xattrs == NULL ) {
      return 0;
   }
   
   for( xattr = fskit_xattr_set_begin( &itr, xattrs ); xattr != NULL; xattr = fskit_xattr_set_next( &itr ) ) {
      
      name = fskit_xattr_set_name( xattr );
      size += strlen(name) + 1;
   }

   return size;
}


// copy the xattr names into a buffer.
// NOTE: no input validation occurs here.  The caller must ensure that list has enough space to hold all names, separated by \0's
static void fskit_listxattr_copy_names( fskit_xattr_set* xattrs, char* list, size_t size ) {

   off_t offset = 0;
   char const* name = NULL;
   fskit_xattr_set_itr itr;
   fskit_xattr_set* xattr = NULL;
   
   for( xattr = fskit_xattr_set_begin( &itr, xattrs ); xattr != NULL; xattr = fskit_xattr_set_next( &itr ) ) {
      
      name = fskit_xattr_set_name( xattr );
      memcpy( list + offset, name, strlen(name) );
      
      offset += strlen(name);
      *(list + offset) = '\0';
      offset++;
   }
}


// low-level listxattr 
// return length copied on success (or if size is 0)
// return -ERANGE if the buffer is too short
// fent must be read-locked
int fskit_xattr_flistxattr( struct fskit_core* core, struct fskit_entry* fent, char* list, size_t size ) {

   int total_size = 0;

   total_size = fskit_listxattr_len( fent->xattrs );

   // just a length query?
   if( list == NULL || size == 0 ) {
      return total_size;
   }

   // range check
   if( (unsigned)total_size > size ) {
      return -ERANGE;
   }

   // copy new names in
   fskit_listxattr_copy_names( fent->xattrs, list, size );
   return total_size;
}


// get the list of all xattr names
// return the length of the name list on success
// return on error:
// * -ERANGE if the buffer is too short
// * -EPERM if the callback failed
// if list == NULL or size == 0, then just return the length of the name list
// NOTE: fent must be at least read-locked
int fskit_flistxattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, char* list, size_t size ) {

   int total_size = 0;
   int user_size = 0;
   int rc = 0;

   // can the callback service this?
   rc = fskit_run_user_listxattr( core, path, fent, list, size );
   if( rc < 0 ) {
      fskit_error("fskit_run_user_listxattr('%s') rc = %d\n", path, rc );
      return rc;
   }

   if( rc > 0 ) {
      // callback handled 
      user_size = rc;
      return user_size;
   }

   // not handled
   return fskit_xattr_flistxattr( core, fent, list, size );
}
