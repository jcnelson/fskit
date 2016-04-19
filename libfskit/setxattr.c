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

#include <fskit/setxattr.h>
#include <fskit/path.h>

#include "fskit_private/private.h"

// get the user-supplied xattr data for setxattr
// return 0 if the callback handled the request
// return 1 if the callback did not handle the request, and fskit should instead
// return negative on error
int fskit_run_user_setxattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, char const* xattr_name, char const* xattr_value, size_t xattr_value_len, int flags ) {

   int rc = 0;
   int cbrc = 0;
   struct fskit_route_dispatch_args dargs;
   
   fskit_route_setxattr_args( &dargs, xattr_name, xattr_value, xattr_value_len, flags );
   rc = fskit_route_call_setxattr( core, path, fent, &dargs, &cbrc );

   if( rc == -EPERM ) {
      // no routes 
      return 1;
   }
   else if( rc < 0 ) {
      return rc;
   }

   return cbrc;
}


// set an xattr by path.
// returns whatever fsetxattr returns, plus any errors that can result from path resolution.
int fskit_setxattr( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, char const* name, char const* value, size_t value_len, int flags ) {

   int err = 0;
   int rc = 0;

   // get the fent
   struct fskit_entry* fent = fskit_entry_resolve_path( core, path, user, group, true, &err );
   if( fent == NULL || err != 0 ) {
      return err;
   }

   // set the xattr
   rc = fskit_fsetxattr( core, path, fent, name, value, value_len, flags );

   fskit_entry_unlock( fent );

   return rc;
}


// set an xattr value directly in the inode
// the inode must be write-locked
// return 0 on success
// return -ENOSPC if out-of-memory
// return -EEXIST if flags has XATTR_CREATE set and the attribute already exists
// return -ENOATTR if flags has XATTR_REPALCE set and the attribute does not exist
int fskit_xattr_fsetxattr( struct fskit_core* core, struct fskit_entry* fent, char const* name, char const* value, size_t value_len, int flags ) {

   int rc = 0;
   rc = fskit_xattr_set_insert( &fent->xattrs, name, value, value_len, flags );
   if( rc == -ENOMEM ) {
      
      rc = -ENOSPC;
   }
   
   return rc;
}


// set an xattr value on a fskit_entry.
// fent must be at least read-locked
// on success, return 0
// on error:
// * return -EINVAL if both XATTR_CREATE and XATTR_REPLACE are set
// * if XATTR_CREATE is set and the attribute already exists, return EEXIST
// * if XATTR_REPLACE is set and the attribute does not exist, return ENOATTR
// * if there is no memory, return -ENOSPC
// NOTE: fent must be write-locked
int fskit_fsetxattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, char const* name, char const* value, size_t value_len, int flags ) {

   int rc = 0;
   
   // check for invalid flags
   if( (flags & XATTR_REPLACE) && (flags & XATTR_CREATE) ) {
      return -EINVAL;
   }

   // see if callback will handle this 
   rc = fskit_run_user_setxattr( core, path, fent, name, value, value_len, flags );
   if( rc < 0 ) {
      fskit_error("fskit_run_user_setxattr('%s', '%s') rc = %d\n", path, name, rc );
      return -EPERM;
   }

   if( rc == 0 ) {
      // take no further action 
      return 0;
   }

   // callback forwards to fskit
   rc = fskit_xattr_fsetxattr( core, fent, name, value, value_len, flags );
   
   return rc;
}
