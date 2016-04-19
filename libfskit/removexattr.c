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

#include <fskit/removexattr.h>
#include <fskit/path.h>
#include <fskit/util.h>

#include "fskit_private/private.h"

// do user-supplied removexattr logic
// return 0 if handled by the callback
// return 1 if not
// return negative on error
int fskit_run_user_removexattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, char const* xattr_name ) {

   int rc = 0;
   int cbrc = 0;
   struct fskit_route_dispatch_args dargs;
   
   fskit_route_removexattr_args( &dargs, xattr_name);
   rc = fskit_route_call_removexattr( core, path, fent, &dargs, &cbrc );

   if( rc == -EPERM ) {
      // no routes
      return 0;
   }
   else if( rc < 0 ) {
      return rc;
   }

   return cbrc;
}

// remove an xattr.
// returns whatever fremovexattr returns, plus whatever error codes path resolution can return
int fskit_removexattr( struct fskit_core* core, char const* path, uint64_t user, uint64_t group, char const* name ) {

   int err = 0;
   int rc = 0;

   // get the fent
   struct fskit_entry* fent = fskit_entry_resolve_path( core, path, user, group, true, &err );
   if( fent == NULL || err != 0 ) {
      return err;
   }

   // get the xattr
   rc = fskit_fremovexattr( core, path, fent, name );

   fskit_entry_unlock( fent );

   return rc;
}


// remove an xattr from a write-locked inode.  do not call the route
// return 0 on success
// return -ENOATTR if the attribute doesn't exist 
int fskit_xattr_fremovexattr( struct fskit_core* core, struct fskit_entry* fent, char const* name ) {

   bool removed = false;
   
   removed = fskit_xattr_set_remove( &fent->xattrs, name );
   if( removed ) {
      
      return 0;
   }
   else {
      
      return -ENOATTR;
   }
}

// remove an xattr.
// return 0 on success
// return -ENOATTR if the attribute doesn't exist
// return -EPERM if the user-given callback fails
// NOTE: fent must be write-locked
int fskit_fremovexattr( struct fskit_core* core, char const* path, struct fskit_entry* fent, char const* name ) {

   int rc = 0;
   bool removed = false;
   
   rc = fskit_run_user_removexattr( core, path, fent, name );
   if( rc < 0 ) {
      fskit_error("fskit_user_removexattr('%s', '%s') rc = %d\n", path, name, rc );
      return -EPERM;
   }
   
   if( rc == 0 ) {
      // removed 
      return 0;
   }

   return fskit_xattr_fremovexattr( core, fent, name );
}


// clear out all xattrs at once 
// return 0 on success
// return -ENOMEM on OOM
// NOTE: fent must be write-locked
int fskit_fremovexattr_all( struct fskit_core* core, struct fskit_entry* fent ) {
   
   fskit_xattr_set* old_xattrs = NULL;
   fskit_xattr_set* new_xattrs = CALLOC_LIST( fskit_xattr_set, 1 );
   
   if( new_xattrs == NULL ) {
      
      return -ENOMEM;
   }
   
   old_xattrs = fent->xattrs;
   fent->xattrs = new_xattrs;
   
   fskit_xattr_set_free( old_xattrs );
   
   return 0;
}

